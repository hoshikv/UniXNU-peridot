// SPDX-License-Identifier: BSD-2-Clause-Patent
//
// Resident polled Qualcomm USB fastboot server for DarwinBoot.
//
// The UEFI image stays alive on the boot CPU.  XNU is started on a secondary
// CPU with the MMU and caches off; the small stubs below grad a shared log and
// hand control back to this loop.  Nothing here calls ExitBootServices().
//
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/DevicePath.h>
#include "BootArgs.h"
#include "Fastboot.h"

#define QCOM_USB_DEVICE_PROTOCOL_GUID \
  { 0xd9d9ce48, 0x44b8, 0x4f49, { 0x8e, 0x3e, 0x2a, 0x3b, 0x92, 0x7d, 0xc6, 0xc1 } }
#define QCOM_USB_INIT_EVENT_GUID \
  { 0x1c0cffce, 0xfc8d, 0x4e44, { 0x8c, 0x78, 0x9c, 0x9e, 0x5b, 0x53, 0x0d, 0x36 } }

#define USB_EP_OUT                0x01
#define USB_EP_IN                 0x81
#define USB_CHUNK_SIZE            511
#define USB_TRANSFER_SIZE         (1024 * 1024)
#define FASTBOOT_MAX_DOWNLOAD     (64ULL * 1024 * 1024)
#define FASTBOOT_UEFI_LOG_SIZE    (64 * 1024)
#define FASTBOOT_KERNEL_LOG_MAX   (1024 * 1024)
#define FASTBOOT_STAGED_SIZE      (FASTBOOT_UEFI_LOG_SIZE + FASTBOOT_KERNEL_LOG_MAX + 8192)

#define FASTBOOT_MENU_ENTRIES     5
#define FASTBOOT_MENU_LOG_WINDOW  1536

#ifndef SCAN_SUSPEND
#define SCAN_SUSPEND              0x0102
#endif

#define FASTBOOT_SLOT_MAGIC       0x545352444c554e58ULL  // "XNULDRST"
#define FASTBOOT_LIVE_MAGIC       0x214556494c554e58ULL  // "XNULIVE!"
#define FASTBOOT_PSCI_CPU_OFF     0x84000002ULL
#define FASTBOOT_PSCI_CPU_ON      0xC4000003ULL
#define FASTBOOT_PSCI_ALREADY_ON  (-4)

enum {
  QCOM_USB_EVENT_NONE,
  QCOM_USB_EVENT_DEVICE_STATE_CHANGE,
  QCOM_USB_EVENT_TRANSFER_NOTIFICATION,
  QCOM_USB_EVENT_OEM
};

enum {
  QCOM_USB_TRANSFER_ACTIVE,
  QCOM_USB_TRANSFER_COMPLETE_OK,
  QCOM_USB_TRANSFER_CANCELLED,
  QCOM_USB_TRANSFER_COMPLETE_ERROR
};

enum {
  QCOM_USB_STATE_CONNECTED,
  QCOM_USB_STATE_DISCONNECTED
};

typedef union {
  UINT32  DeviceState;
  struct {
    UINT32  Status;
    UINT8   EndpointIndex;
    UINT64  BytesCompleted;
    VOID    *DataBuffer;
  } Transfer;
  UINT64  Raw[8];
} QCOM_USB_DEVICE_EVENT_DATA;

typedef struct {
  CONST VOID  *DeviceDescriptor;
  CONST VOID  **Descriptors;
  CONST VOID  *SuperSpeedDeviceDescriptor;
  CONST VOID  **SuperSpeedDescriptors;
  CONST VOID  *DeviceQualifier;
  CONST VOID  *Bos;
  UINT8       StringCount;
  CONST VOID  **Strings;
} QCOM_USB_DEVICE_DESCRIPTOR_SET;

typedef
EFI_STATUS
(EFIAPI *QCOM_USB_SEND)(
  IN UINT8  Endpoint,
  IN UINTN  Size,
  IN VOID   *Buffer
  );

typedef
EFI_STATUS
(EFIAPI *QCOM_USB_HANDLE_EVENT)(
  IN  UINT32                      *Event,
  IN  UINT64                      *Size,
  OUT QCOM_USB_DEVICE_EVENT_DATA  *Data
  );

typedef
EFI_STATUS
(EFIAPI *QCOM_USB_ALLOCATE_TRANSFER_BUFFER)(
  IN  UINTN  Size,
  OUT VOID   **Buffer
  );

typedef
EFI_STATUS
(EFIAPI *QCOM_USB_FREE_TRANSFER_BUFFER)(
  IN VOID  *Buffer
  );

typedef
EFI_STATUS
(EFIAPI *QCOM_USB_START_EX)(
  IN QCOM_USB_DEVICE_DESCRIPTOR_SET  *DescriptorSet
  );

typedef struct {
  UINT64                            Revision;
  VOID                              *Start;
  QCOM_USB_SEND                     Send;
  QCOM_USB_HANDLE_EVENT             HandleEvent;
  QCOM_USB_ALLOCATE_TRANSFER_BUFFER AllocateTransferBuffer;
  QCOM_USB_FREE_TRANSFER_BUFFER     FreeTransferBuffer;
  VOID                              *Stop;
  VOID                              *AbortXfer;
  VOID                              *SetEndpointStallState;
  QCOM_USB_START_EX                 StartEx;
} QCOM_USB_DEVICE_PROTOCOL;

STATIC CONST UINT8 mUsbDeviceDescriptor[18] = {
  18, 0x01, 0x10, 0x02, 0x00, 0x00, 0x00, 64,
  0xd1, 0x18, 0x0d, 0xd0, 0x00, 0x01, 1, 2, 3, 1
};
STATIC CONST UINT8 mUsbSuperSpeedDeviceDescriptor[18] = {
  18, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 9,
  0xd1, 0x18, 0x0d, 0xd0, 0x00, 0x01, 1, 2, 3, 1
};
STATIC CONST UINT8 mUsbConfigDescriptor[32] = {
  9, 0x02, 32, 0, 1, 1, 0, 0x80, 0x50,
  9, 0x04, 0, 0, 2, 0xff, 0x42, 0x03, 4,
  7, 0x05, USB_EP_IN, 2, 0x00, 0x02, 0,
  7, 0x05, USB_EP_OUT, 2, 0x00, 0x02, 0
};
STATIC CONST UINT8 mUsbSuperSpeedConfigDescriptor[44] = {
  9, 0x02, 44, 0, 1, 1, 0, 0x80, 0x10,
  9, 0x04, 0, 0, 2, 0xff, 0x42, 0x03, 4,
  7, 0x05, USB_EP_IN, 2, 0x00, 0x04, 0, 6, 0x30, 4, 0, 0, 0,
  7, 0x05, USB_EP_OUT, 2, 0x00, 0x04, 0, 6, 0x30, 4, 0, 0, 0
};
STATIC CONST UINT8 mUsbDeviceQualifier[10] = {
  10, 0x06, 0x00, 0x02, 0xff, 0xff, 0xff, 64, 1, 0
};
STATIC CONST UINT8 mUsbBos[42] = {
  5, 0x0f, 42, 0, 3,
  7, 0x10, 0x02, 0x06, 0, 0, 0,
  10, 0x10, 0x03, 0x00, 0x0e, 0x00, 0x01, 0x07, 0x65, 0x00,
  20, 0x10, 0x0a, 0x00, 0x01, 0, 0, 0, 0x00, 0x11, 0, 0, 0x30, 0x40, 0x0a, 0x00, 0xb0, 0x40, 0x0a, 0x00
};
STATIC CONST UINT8 mUsbString0[4] = { 4, 0x03, 0x09, 0x04 };
STATIC CONST UINT8 mUsbStringManufacturer[8] = { 8, 0x03, 'X', 0, 'N', 0, 'U', 0 };
STATIC CONST UINT8 mUsbStringProduct[20] = {
  20, 0x03, 'x', 0, 'n', 0, 'u', 0, 'l', 0, 'o', 0, 'a', 0, 'd', 0, 'e', 0, 'r', 0
};
STATIC CONST UINT8 mUsbStringSerial[20] = {
  20, 0x03, 'x', 0, 'n', 0, 'u', 0, 'l', 0, 'o', 0, 'a', 0, 'd', 0, 'e', 0, 'r', 0
};
STATIC CONST UINT8 mUsbStringInterface[18] = {
  18, 0x03, 'f', 0, 'a', 0, 's', 0, 't', 0, 'b', 0, 'o', 0, 'o', 0, 't', 0
};

STATIC CONST VOID *mUsbConfigDescriptors[1] = { mUsbConfigDescriptor };
STATIC CONST VOID *mUsbSuperSpeedConfigDescriptors[1] = { mUsbSuperSpeedConfigDescriptor };
STATIC CONST VOID *mUsbStrings[5] = {
  mUsbString0,
  mUsbStringManufacturer,
  mUsbStringProduct,
  mUsbStringSerial,
  mUsbStringInterface
};

STATIC QCOM_USB_DEVICE_DESCRIPTOR_SET mDescriptors = {
  mUsbDeviceDescriptor,
  mUsbConfigDescriptors,
  mUsbSuperSpeedDeviceDescriptor,
  mUsbSuperSpeedConfigDescriptors,
  mUsbDeviceQualifier,
  mUsbBos,
  5,
  mUsbStrings
};

typedef struct {
  UINT64  Magic;
  UINT64  Heartbeat;
  UINT64  Log;
  UINT64  LogLength;
  UINT64  Reason;
  volatile UINT64 Stop;
  UINT64  Reserved[10];
} KERNEL_LIVE;

typedef struct {
  UINT64  Entry;
  UINT64  BootArgs;
  UINT64  Cpacr;
  UINT64  Sctlr;
  UINT64  Context;
  UINT64  StackTop;
} FASTBOOT_START_BLOCK;

STATIC EFI_GUID mQcomUsbGuid = QCOM_USB_DEVICE_PROTOCOL_GUID;
STATIC EFI_GUID mQcomUsbInitGuid = QCOM_USB_INIT_EVENT_GUID;

STATIC QCOM_USB_DEVICE_PROTOCOL *mUsb;
STATIC EFI_EVENT                 mUsbInitEvent;
STATIC BOOLEAN                   mUsbConnected;
STATIC BOOLEAN                   mUsbReading;
STATIC UINTN                     mUsbQueuedLength;
STATIC VOID                      *mUsbRx;
STATIC VOID                      *mUsbTx;

STATIC UINT64  mArgsAddress;
STATIC UINT64  mEntryAddress;
STATIC UINT64  mMemoryBase;
STATIC UINT64  mMemorySize;
STATIC UINT64  mKernelBase;
STATIC UINT64  mKernelSize;
STATIC UINT64  mKernelSlide;
STATIC UINT64  mKernelLivePa;
STATIC UINT64  mVideo[6];
STATIC UINT64  mCheckpoint;
STATIC BOOLEAN mBusy;
STATIC BOOLEAN mStarted;
STATIC CHAR8   mResult[64];
STATIC BOOLEAN mMenuInput;
STATIC BOOLEAN mMenuVisible;
STATIC BOOLEAN mMenuDirty;
STATIC UINTN   mMenuSelection;

STATIC CHAR8   mUefiLog[FASTBOOT_UEFI_LOG_SIZE];
STATIC UINTN   mUefiLogLength;
STATIC UINT8   mStaged[FASTBOOT_STAGED_SIZE];
STATIC UINTN   mStagedLength;

STATIC CHAR8   mCommand[USB_CHUNK_SIZE + 1];

// Referenced by FastbootEntry.S.  The return stub runs with the MMU and caches
// off, so these must be identity-mapped UEFI data addresses.
volatile UINT64 gFastbootDone;
UINT64          gFastbootContext[8];
UINT8           gFastbootSecondaryStack[0x4000] __attribute__ ((aligned (16)));

extern VOID FastbootOtherCpuEntry (VOID);
extern VOID FastbootOtherCpuReturnStub (VOID);

STATIC
VOID
FastbootLog (
  IN CONST CHAR8  *Format,
  ...
  )
{
  VA_LIST Marker;
  CHAR8   Buffer[256];
  UINTN   Length;

  VA_START (Marker, Format);
  Length = AsciiVSPrint (Buffer, sizeof (Buffer), Format, Marker);
  VA_END (Marker);
  if (Length >= sizeof (Buffer)) {
    Length = sizeof (Buffer) - 1;
  }
  if (mUefiLogLength + Length < sizeof (mUefiLog)) {
    CopyMem (mUefiLog + mUefiLogLength, Buffer, Length);
    mUefiLogLength += Length;
  }
  DEBUG ((DEBUG_INFO, "FASTBOOT: %a", Buffer));
}

STATIC
BOOLEAN
FastbootStartsWith (
  IN CONST CHAR8  *Text,
  IN CONST CHAR8  *Prefix
  )
{
  UINTN Length;

  Length = AsciiStrLen (Prefix);
  return AsciiStrnCmp (Text, Prefix, Length) == 0;
}

STATIC
BOOLEAN
FastbootEquals (
  IN CONST CHAR8  *Text,
  IN CONST CHAR8  *Value
  )
{
  return AsciiStrCmp (Text, Value) == 0;
}

STATIC
BOOLEAN
FastbootContains (
  IN CONST CHAR8  *Text,
  IN CONST CHAR8  *Needle
  )
{
  UINTN TextLength;
  UINTN NeedleLength;
  UINTN Index;

  TextLength = AsciiStrLen (Text);
  NeedleLength = AsciiStrLen (Needle);
  if (NeedleLength == 0) {
    return TRUE;
  }
  if (TextLength < NeedleLength) {
    return FALSE;
  }
  for (Index = 0; Index <= TextLength - NeedleLength; Index++) {
    if (AsciiStrnCmp (Text + Index, Needle, NeedleLength) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

STATIC
VOID
FastbootCleanRange (
  IN UINT64  Address,
  IN UINT64  Length
  )
{
  UINT64 Current;

  if (Length == 0) {
    return;
  }
  for (Current = Address & ~0x3FULL; Current < Address + Length; Current += 64) {
    __asm__ __volatile__ ("dc cvac, %0" : : "r" (Current) : "memory");
  }
  __asm__ __volatile__ ("dsb sy" : : : "memory");
}

STATIC
VOID
FastbootInvalidateRange (
  IN UINT64  Address,
  IN UINT64  Length
  )
{
  UINT64 Current;

  if (Length == 0) {
    return;
  }
  for (Current = Address & ~0x3FULL; Current < Address + Length; Current += 64) {
    __asm__ __volatile__ ("dc civac, %0" : : "r" (Current) : "memory");
  }
  __asm__ __volatile__ ("dsb sy" : : : "memory");
}

STATIC
VOID
FastbootIcacheInvalidate (
  VOID
  )
{
  __asm__ __volatile__ ("ic iallu" : : : "memory");
  __asm__ __volatile__ ("dsb sy" : : : "memory");
  __asm__ __volatile__ ("isb" : : : "memory");
}

STATIC
UINT64
FastbootReadSctlr (
  VOID
  )
{
  UINT64 Value;

  __asm__ __volatile__ ("mrs %0, sctlr_el1" : "=r" (Value));
  return Value;
}

STATIC
UINT64
FastbootReadCpacr (
  VOID
  )
{
  UINT64 Value;

  __asm__ __volatile__ ("mrs %0, cpacr_el1" : "=r" (Value));
  return Value;
}

STATIC
INT64
FastbootPsci (
  IN UINT64  Function,
  IN UINT64  Arg1,
  IN UINT64  Arg2,
  IN UINT64  Arg3
  )
{
  register UINT64 X0 __asm__ ("x0") = Function;
  register UINT64 X1 __asm__ ("x1") = Arg1;
  register UINT64 X2 __asm__ ("x2") = Arg2;
  register UINT64 X3 __asm__ ("x3") = Arg3;

  __asm__ __volatile__ (
    "smc #0"
    : "+r" (X0), "+r" (X1), "+r" (X2), "+r" (X3)
    :
    : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
      "x12", "x13", "x14", "x15", "x16", "x17", "memory"
    );
  return (INT64)X0;
}

STATIC
UINT64 *
FastbootFindMagic (
  IN UINT64  Base,
  IN UINT64  Size,
  IN UINT64  Magic
  )
{
  UINT64 Offset;

  for (Offset = 0; Offset + 32 <= Size; Offset += 8) {
    if (*(volatile UINT64 *)(UINTN)(Base + Offset) == Magic) {
      return (UINT64 *)(UINTN)(Base + Offset);
    }
  }
  return NULL;
}

STATIC
VOID
FastbootPatchKernel (
  IN UINT64   Checkpoint,
  IN BOOLEAN  SetCheckpoint
  )
{
  UINT64       *Slot;
  UINT64       *Live;
  UINT64       Count;
  KERNEL_LIVE  *KernelLive;

  Slot = FastbootFindMagic (mMemoryBase, mKernelSize, FASTBOOT_SLOT_MAGIC);
  Live = FastbootFindMagic (mMemoryBase, mKernelSize, FASTBOOT_LIVE_MAGIC);
  mKernelLivePa = (UINT64)(UINTN)Live;
  if (Live != NULL) {
    KernelLive = (KERNEL_LIVE *)(UINTN)Live;
    KernelLive->Reason = 0;
    KernelLive->Stop = 0;
    FastbootCleanRange (mKernelLivePa, sizeof (KERNEL_LIVE));
    FastbootLog ("kernel live block at 0x%lx\n", mKernelLivePa);
  } else {
    FastbootLog ("kernel live block not found\n");
  }

  if (Slot != NULL) {
    Slot[1] = (UINT64)(UINTN)&FastbootOtherCpuReturnStub;
    Slot[2] = SetCheckpoint ? Checkpoint : 0;
    Slot[3] = mKernelSlide;
    FastbootCleanRange ((UINT64)(UINTN)Slot, 32);
    FastbootLog ("loader slot at 0x%lx checkpoint %lu\n", (UINT64)(UINTN)Slot, Slot[2]);
  } else {
    FastbootLog ("loader slot not found\n");
  }

  Count = 0;
  if (Slot != NULL) {
    Count++;
  }
  if (Live != NULL) {
    Count++;
  }
  FastbootLog ("kernel scan: %lu structures found\n", Count);
}

STATIC
VOID
FastbootUpdateContext (
  VOID
  )
{
  if (!mBusy) {
    return;
  }
  FastbootInvalidateRange ((UINT64)(UINTN)&gFastbootDone, sizeof (gFastbootDone));
  if (gFastbootDone == 0) {
    return;
  }
  FastbootInvalidateRange ((UINT64)(UINTN)gFastbootContext, sizeof (gFastbootContext));
  mBusy = FALSE;
  switch (gFastbootContext[2]) {
    case 0:
      AsciiStrCpyS (mResult, sizeof (mResult), "returned");
      break;
    case 1:
      AsciiStrCpyS (mResult, sizeof (mResult), "early-exception");
      break;
    case 2:
      AsciiStrCpyS (mResult, sizeof (mResult), "checkpoint");
      break;
    default:
      AsciiSPrint (mResult, sizeof (mResult), "stopped-%lu", gFastbootContext[2]);
      break;
  }
  FastbootLog ("kernel stopped: %a esr=%lx elr=%lx far=%lx\n",
               mResult, gFastbootContext[3], gFastbootContext[4], gFastbootContext[5]);
  mMenuVisible = TRUE;
  mMenuDirty = TRUE;
}

STATIC
UINTN
FastbootCopyKernelLog (
  OUT UINT8  *Dst,
  IN  UINTN  Max
  )
{
  UINT64       LogPa;
  UINT64       LogLength;
  UINT64       LogLengthPa;
  KERNEL_LIVE  *KernelLive;

  LogPa = 0;
  LogLength = 0;
  if ((gFastbootContext[0] != 0) && (gFastbootContext[1] != 0)) {
    LogPa = gFastbootContext[0];
    LogLength = gFastbootContext[1];
  } else if (mKernelLivePa != 0) {
    FastbootInvalidateRange (mKernelLivePa, sizeof (KERNEL_LIVE));
    KernelLive = (KERNEL_LIVE *)(UINTN)mKernelLivePa;
    if ((KernelLive->Magic == FASTBOOT_LIVE_MAGIC) && (KernelLive->LogLength != 0)) {
      LogLengthPa = KernelLive->LogLength + mKernelSlide;
      FastbootInvalidateRange (LogLengthPa, sizeof (UINT32));
      LogLength = *(volatile UINT32 *)(UINTN)LogLengthPa;
      LogPa = KernelLive->Log + mKernelSlide;
    }
  }
  if ((LogPa == 0) || (LogLength == 0)) {
    return 0;
  }
  if (LogLength > Max) {
    LogLength = Max;
  }
  FastbootInvalidateRange (LogPa, LogLength);
  CopyMem (Dst, (VOID *)(UINTN)LogPa, (UINTN)LogLength);
  return (UINTN)LogLength;
}

STATIC
VOID
FastbootStageLogs (
  VOID
  )
{
  UINTN Used;
  UINTN Length;
  INTN  Printed;

  FastbootUpdateContext ();
  Used = 0;
  mStagedLength = 0;
  Printed = AsciiSPrint (
              (CHAR8 *)mStaged,
              sizeof (mStaged),
              "UEFI fastboot log\nbusy=%u started=%u result=%a checkpoint=%lu\n",
              mBusy, mStarted, mResult[0] ? mResult : "none", mCheckpoint
              );
  Used = (Printed > 0) ? (UINTN)Printed : 0;
  if (Used >= sizeof (mStaged)) {
    Used = sizeof (mStaged) - 1;
  }
  if (mUefiLogLength != 0) {
    Length = MIN (mUefiLogLength, sizeof (mStaged) - Used - 1);
    CopyMem (mStaged + Used, mUefiLog, Length);
    Used += Length;
  }
  if (Used + 16 < sizeof (mStaged)) {
    CopyMem (mStaged + Used, "\nkernel log:\n", 13);
    Used += 13;
  }
  Length = FastbootCopyKernelLog (mStaged + Used, sizeof (mStaged) - Used - 1);
  Used += Length;
  if (Used + 6 < sizeof (mStaged)) {
    CopyMem (mStaged + Used, "\nend\n", 5);
    Used += 5;
  }
  mStaged[Used] = 0;
  mStagedLength = Used;
  FastbootLog ("staged %lu log bytes\n", mStagedLength);
}

STATIC
INTN
FastbootUsbWait (
  IN UINT8  Endpoint
  )
{
  UINT32                      Event;
  UINT64                      Size;
  QCOM_USB_DEVICE_EVENT_DATA  Data;
  EFI_STATUS                  Status;

  for (;;) {
    Event = QCOM_USB_EVENT_NONE;
    Size = 0;
    ZeroMem (&Data, sizeof (Data));
    Status = mUsb->HandleEvent (&Event, &Size, &Data);
    if (EFI_ERROR (Status)) {
      return -1;
    }
    if (Event == QCOM_USB_EVENT_DEVICE_STATE_CHANGE) {
      mUsbConnected = (Data.DeviceState == QCOM_USB_STATE_CONNECTED);
      Print (L"DARWINBOOT: USB host %s\n",
             mUsbConnected ? L"connected" : L"disconnected");
      mMenuDirty = TRUE;
      return -1;
    }
    if ((Event == QCOM_USB_EVENT_TRANSFER_NOTIFICATION) &&
        (Data.Transfer.EndpointIndex == Endpoint)) {
      if (Data.Transfer.Status != QCOM_USB_TRANSFER_COMPLETE_OK) {
        return -1;
      }
      return (INTN)Data.Transfer.BytesCompleted;
    }
    gBS->Stall (100);
  }
}

STATIC
EFI_STATUS
FastbootUsbTxRaw (
  IN CONST VOID  *Data,
  IN UINTN       Length
  )
{
  CONST UINT8  *Bytes;
  UINTN        Chunk;
  INTN         Completed;
  EFI_STATUS   Status;

  if (mUsb == NULL) {
    return EFI_NOT_READY;
  }
  Bytes = Data;
  while (Length != 0) {
    Chunk = MIN (Length, USB_TRANSFER_SIZE);
    CopyMem (mUsbTx, Bytes, Chunk);
    Status = mUsb->Send (USB_EP_IN, Chunk, mUsbTx);
    if (EFI_ERROR (Status)) {
      FastbootLog ("usb tx failed: %r\n", Status);
      return Status;
    }
    Completed = FastbootUsbWait (USB_EP_IN);
    if (Completed < 0) {
      FastbootLog ("usb tx did not complete\n");
      return EFI_DEVICE_ERROR;
    }
    Bytes += Chunk;
    Length -= Chunk;
  }
  return EFI_SUCCESS;
}

STATIC
VOID
FastbootReply (
  IN CONST CHAR8  *StatusText,
  IN CONST CHAR8  *Info
  )
{
  CHAR8  Reply[256];
  UINTN  Length;

  Length = AsciiSPrint (Reply, sizeof (Reply), "%a%a", StatusText, Info);
  if (Length >= sizeof (Reply)) {
    Length = sizeof (Reply) - 1;
  }
  FastbootUsbTxRaw (Reply, Length);
}

STATIC
VOID
FastbootFail (
  IN CONST CHAR8  *Info
  )
{
  FastbootReply ("FAIL", Info);
}

STATIC
VOID
FastbootUploadLog (
  VOID
  )
{
  CHAR8  Header[16];
  UINTN  HeaderLength;

  FastbootStageLogs ();
  HeaderLength = AsciiSPrint (Header, sizeof (Header), "DATA%08x", (UINT32)mStagedLength);
  if (HeaderLength != 12) {
    return;
  }
  FastbootUsbTxRaw (Header, HeaderLength);
  FastbootUsbTxRaw (mStaged, mStagedLength);
}

STATIC
EFI_STATUS
FastbootUsbRead (
  IN  BOOLEAN  Blocking,
  IN  UINTN    MaxLength,
  OUT UINTN    *Length
  )
{
  UINT32                      Event;
  UINT64                      Size;
  QCOM_USB_DEVICE_EVENT_DATA  Data;
  EFI_STATUS                  Status;

  if (mUsb == NULL) {
    return EFI_NOT_READY;
  }
  if (MaxLength > USB_TRANSFER_SIZE) {
    MaxLength = USB_TRANSFER_SIZE;
  }
  if (!mUsbReading) {
    ZeroMem (mUsbRx, MaxLength);
    Status = mUsb->Send (USB_EP_OUT, MaxLength, mUsbRx);
    if (EFI_ERROR (Status)) {
      FastbootLog ("usb rx queue failed: %r\n", Status);
      return Status;
    }
    mUsbQueuedLength = MaxLength;
    mUsbReading = TRUE;
  }
  for (;;) {
    Event = QCOM_USB_EVENT_NONE;
    Size = 0;
    ZeroMem (&Data, sizeof (Data));
    Status = mUsb->HandleEvent (&Event, &Size, &Data);
    if (EFI_ERROR (Status)) {
      mUsbReading = FALSE;
      return Status;
    }
    if (Event == QCOM_USB_EVENT_DEVICE_STATE_CHANGE) {
      mUsbConnected = (Data.DeviceState == QCOM_USB_STATE_CONNECTED);
      mUsbReading = FALSE;
      mMenuDirty = TRUE;
      return EFI_NOT_READY;
    }
    if ((Event == QCOM_USB_EVENT_TRANSFER_NOTIFICATION) &&
        (Data.Transfer.EndpointIndex == USB_EP_OUT)) {
      mUsbReading = FALSE;
      if ((Data.Transfer.Status != QCOM_USB_TRANSFER_COMPLETE_OK) ||
          (Data.Transfer.BytesCompleted == 0)) {
        return EFI_DEVICE_ERROR;
      }
      *Length = (UINTN)MIN (Data.Transfer.BytesCompleted, mUsbQueuedLength);
      return EFI_SUCCESS;
    }
    if (!Blocking) {
      return EFI_NOT_READY;
    }
    gBS->Stall (1000);
  }
}

STATIC
EFI_STATUS
FastbootStartKernel (
  IN UINT64  Checkpoint
  )
{
  FASTBOOT_START_BLOCK  Start;
  INT64                 PsciStatus;
  UINT64                Core;
  UINT64                StartAddress;

  if (mBusy || mStarted) {
    return EFI_ACCESS_DENIED;
  }
  FastbootPatchKernel (Checkpoint, TRUE);
  FastbootLog ("starting kernel entry=0x%lx args=0x%lx slide=0x%lx\n",
               mEntryAddress, mArgsAddress, mKernelSlide);
  ZeroMem (&Start, sizeof (Start));
  ZeroMem ((VOID *)gFastbootContext, sizeof (gFastbootContext));
  gFastbootDone = 0;
  Start.Entry = mEntryAddress;
  Start.BootArgs = mArgsAddress;
  Start.Cpacr = FastbootReadCpacr ();
  Start.Sctlr = FastbootReadSctlr ();
  Start.Context = (UINT64)(UINTN)gFastbootContext;
  Start.StackTop = (UINT64)(UINTN)(gFastbootSecondaryStack + sizeof (gFastbootSecondaryStack));
  StartAddress = (UINT64)(UINTN)&Start;

  FastbootCleanRange ((UINT64)(UINTN)&FastbootOtherCpuEntry,
                      (UINT64)(UINTN)&FastbootOtherCpuReturnStub + 0x200 -
                      (UINT64)(UINTN)&FastbootOtherCpuEntry);
  FastbootCleanRange ((UINT64)(UINTN)gFastbootSecondaryStack, sizeof (gFastbootSecondaryStack));
  FastbootCleanRange ((UINT64)(UINTN)&Start, sizeof (Start));
  FastbootCleanRange ((UINT64)(UINTN)gFastbootContext, sizeof (gFastbootContext));
  FastbootCleanRange ((UINT64)(UINTN)&gFastbootDone, sizeof (gFastbootDone));
  FastbootCleanRange (mMemoryBase, mMemorySize);
  FastbootIcacheInvalidate ();

  mBusy = TRUE;
  mResult[0] = 0;
  for (Core = 7; Core >= 1; Core--) {
    PsciStatus = FastbootPsci (FASTBOOT_PSCI_CPU_ON, Core, (UINT64)(UINTN)&FastbootOtherCpuEntry, StartAddress);
    if (PsciStatus == 0) {
      mStarted = TRUE;
      mCheckpoint = Checkpoint;
      AsciiStrCpyS (mResult, sizeof (mResult), "running");
      mMenuVisible = FALSE;
      mMenuDirty = FALSE;
      FastbootLog ("kernel on cpu%lu checkpoint %lu\n", Core, Checkpoint);
      if (gST->ConOut != NULL) {
        Print (L"\nDARWINBOOT: kernel started; press a key for the menu\n");
      }
      return EFI_SUCCESS;
    }
    if (PsciStatus == FASTBOOT_PSCI_ALREADY_ON) {
      FastbootLog ("cpu%lu already on\n", Core);
      continue;
    }
    FastbootLog ("cpu%lu CPU_ON returned %ld\n", Core, PsciStatus);
  }
  mBusy = FALSE;
  AsciiStrCpyS (mResult, sizeof (mResult), "not-started");
  FastbootLog ("PSCI CPU_ON failed\n");
  return EFI_DEVICE_ERROR;
}

STATIC
VOID
FastbootDownload (
  IN CONST CHAR8  *Command
  )
{
  UINT64      Size;
  UINT64      Remaining;
  UINTN       Length;
  EFI_STATUS  Status;
  CHAR8       Header[16];
  UINTN       HeaderLength;

  Size = AsciiStrHexToUintn (Command + 9);
  if (Size > FASTBOOT_MAX_DOWNLOAD) {
    FastbootFail ("download too large");
    return;
  }
  HeaderLength = AsciiSPrint (Header, sizeof (Header), "DATA%08x", (UINT32)Size);
  if (HeaderLength != 12) {
    FastbootFail ("download unavailable");
    return;
  }
  FastbootUsbTxRaw (Header, HeaderLength);
  Remaining = Size;
  while (Remaining != 0) {
    Status = FastbootUsbRead (TRUE, USB_TRANSFER_SIZE, &Length);
    if (EFI_ERROR (Status)) {
      FastbootFail ("download read failed");
      return;
    }
    Remaining -= MIN (Remaining, (UINT64)Length);
  }
  FastbootReply ("OKAY", "downloaded\n");
}

STATIC
VOID
FastbootGetVar (
  IN CONST CHAR8  *Name
  )
{
  CHAR8  Value[96];

  if (FastbootEquals (Name, "result")) {
    FastbootReply ("OKAY", mResult[0] ? mResult : "unknown");
  } else if (FastbootEquals (Name, "checkpoint")) {
    AsciiSPrint (Value, sizeof (Value), "%lu", mCheckpoint);
    FastbootReply ("OKAY", Value);
  } else if (FastbootEquals (Name, "product")) {
    FastbootReply ("OKAY", "unixnu-peridot");
  } else if (FastbootEquals (Name, "version")) {
    FastbootReply ("OKAY", "0.1");
  } else if (FastbootEquals (Name, "version-bootloader")) {
    FastbootReply ("OKAY", "DarwinBoot");
  } else if (FastbootEquals (Name, "serialno")) {
    FastbootReply ("OKAY", "peridot");
  } else if (FastbootEquals (Name, "secure")) {
    FastbootReply ("OKAY", "no");
  } else if (FastbootEquals (Name, "max-download-size")) {
    FastbootReply ("OKAY", "0x04000000");
  } else if (FastbootEquals (Name, "kernel")) {
    FastbootReply ("OKAY", mStarted ? "started" : "idle");
  } else if (FastbootEquals (Name, "log-size")) {
    AsciiSPrint (Value, sizeof (Value), "%lu", mStagedLength);
    FastbootReply ("OKAY", Value);
  } else if (FastbootEquals (Name, "all")) {
    FastbootReply ("INFO", "product: unixnu-peridot\n");
    FastbootReply ("INFO", "version: 0.1\n");
    FastbootReply ("INFO", "secure: no\n");
    FastbootReply ("INFO", "max-download-size: 0x04000000\n");
    FastbootReply ("OKAY", "");
  } else {
    FastbootFail ("unknown variable");
  }
}

STATIC
EFI_STATUS
FastbootStopKernel (
  VOID
  )
{
  KERNEL_LIVE  *KernelLive;

  if (!mBusy || (mKernelLivePa == 0)) {
    return EFI_NOT_STARTED;
  }
  FastbootInvalidateRange (mKernelLivePa, sizeof (KERNEL_LIVE));
  KernelLive = (KERNEL_LIVE *)(UINTN)mKernelLivePa;
  KernelLive->Stop = 1;
  FastbootCleanRange (mKernelLivePa, sizeof (KERNEL_LIVE));
  FastbootLog ("kernel stop requested\n");
  mMenuDirty = TRUE;
  return EFI_SUCCESS;
}

STATIC
VOID
FastbootOem (
  IN CONST CHAR8  *Command
  )
{
  CONST CHAR8  *Arg;
  UINT64       Checkpoint;
  EFI_STATUS   Status;

  Arg = Command + 4;
  while (*Arg == ' ') {
    Arg++;
  }
  if (FastbootStartsWith (Arg, "run")) {
    if (mBusy || mStarted) {
      FastbootFail ("kernel already started");
      return;
    }
    Arg += 3;
    while (*Arg == ' ') {
      Arg++;
    }
    Checkpoint = (*Arg != 0) ? AsciiStrDecimalToUintn (Arg) : 0;
    Status = FastbootStartKernel (Checkpoint);
    if (EFI_ERROR (Status)) {
      FastbootFail ("kernel start failed");
    } else {
      FastbootReply ("OKAY", "kernel started\n");
    }
  } else if (FastbootStartsWith (Arg, "log")) {
    FastbootStageLogs ();
    mMenuDirty = TRUE;
    FastbootReply ("OKAY", "log staged\n");
  } else if (FastbootStartsWith (Arg, "stop")) {
    if (EFI_ERROR (FastbootStopKernel ())) {
      FastbootFail ("kernel not running");
      return;
    }
    FastbootReply ("OKAY", "stop requested\n");
  } else if (FastbootStartsWith (Arg, "hang")) {
    FastbootReply ("OKAY", "hanging\n");
    CpuDeadLoop ();
  } else {
    FastbootReply ("INFO", "oem run [checkpoint] | oem log | oem stop | oem hang\n");
    FastbootReply ("OKAY", "");
  }
}

STATIC
VOID
FastbootCommand (
  IN CONST CHAR8  *Command
  )
{
  FastbootLog ("command: %a\n", Command);

  if (FastbootStartsWith (Command, "getvar:")) {
    FastbootGetVar (Command + 7);
  } else if (FastbootStartsWith (Command, "download:")) {
    FastbootDownload (Command);
  } else if (FastbootEquals (Command, "upload") || FastbootEquals (Command, "get_staged")) {
    FastbootUploadLog ();
  } else if (FastbootStartsWith (Command, "oem ")) {
    FastbootOem (Command);
  } else if (FastbootEquals (Command, "boot")) {
    if (mBusy || mStarted) {
      FastbootFail ("kernel already started");
      return;
    }
    if (!EFI_ERROR (FastbootStartKernel (0))) {
      FastbootReply ("OKAY", "kernel started\n");
    } else {
      FastbootFail ("kernel start failed");
    }
  } else if (FastbootEquals (Command, "reboot")) {
    FastbootReply ("OKAY", "rebooting\n");
    gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
    CpuDeadLoop ();
  } else {
    FastbootFail ("unknown command");
  }
}

STATIC
VOID
FastbootServiceUsb (
  VOID
  )
{
  EFI_STATUS  Status;
  UINTN       Length;
  UINTN       Index;

  Status = FastbootUsbRead (FALSE, USB_CHUNK_SIZE, &Length);
  if (Status == EFI_SUCCESS) {
    Index = 0;
    while ((Index < Length) &&
           (((UINT8 *)mUsbRx)[Index] >= 0x20) &&
           (((UINT8 *)mUsbRx)[Index] <= 0x7E)) {
      Index++;
    }
    if (Index != 0) {
      CopyMem (mCommand, mUsbRx, Index);
      mCommand[Index] = 0;
      FastbootCommand (mCommand);
    }
  } else if ((Status != EFI_NOT_READY) && (Status != EFI_TIMEOUT)) {
    FastbootLog ("USB read failed: %r\n", Status);
    gBS->Stall (10000);
  }
  FastbootUpdateContext ();
}

STATIC
BOOLEAN
FastbootMenuInputAvailable (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       Count;
  UINTN       Index;
  UINTN       Physical;
  VOID        *Path;

  if (gST->ConIn == NULL) {
    return FALSE;
  }
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleTextInProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }
  Physical = 0;
  for (Index = 0; Index < Count; Index++) {
    if (!EFI_ERROR (gBS->HandleProtocol (Handles[Index], &gEfiDevicePathProtocolGuid, &Path))) {
      Physical++;
    }
  }
  FreePool (Handles);
  return Physical != 0;
}

STATIC
BOOLEAN
FastbootMenuReadKey (
  OUT EFI_INPUT_KEY  *Key
  )
{
  if (gST->ConIn == NULL) {
    return FALSE;
  }
  return !EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, Key));
}

STATIC
BOOLEAN
FastbootMenuConfirmKey (
  IN CONST EFI_INPUT_KEY  *Key
  )
{
  return (BOOLEAN)((Key->UnicodeChar == CHAR_CARRIAGE_RETURN) ||
                   (Key->UnicodeChar == CHAR_LINEFEED) ||
                   (Key->UnicodeChar == L' ') ||
                   (Key->ScanCode == SCAN_RIGHT) ||
                   (Key->ScanCode == SCAN_SUSPEND));
}

STATIC CONST CHAR16 *mMenuTitles[FASTBOOT_MENU_ENTRIES] = {
  L"Boot kernel",
  L"View kernel log",
  L"Stop kernel",
  L"Restart",
  L"Shut down"
};

STATIC
BOOLEAN
FastbootMenuEnabled (
  IN UINTN  Index
  )
{
  switch (Index) {
    case 0:
      return (BOOLEAN)(!mBusy && !mStarted);
    case 2:
      return mBusy;
    default:
      return TRUE;
  }
}

STATIC
VOID
FastbootMenuMove (
  IN INTN  Delta
  )
{
  INTN  Next;
  UINTN Count;

  Next = (INTN)mMenuSelection;
  for (Count = 0; Count < FASTBOOT_MENU_ENTRIES; Count++) {
    Next += Delta;
    if (Next < 0) {
      Next += FASTBOOT_MENU_ENTRIES;
    } else if (Next >= FASTBOOT_MENU_ENTRIES) {
      Next -= FASTBOOT_MENU_ENTRIES;
    }
    if (FastbootMenuEnabled ((UINTN)Next)) {
      mMenuSelection = (UINTN)Next;
      mMenuDirty = TRUE;
      return;
    }
  }
}

STATIC
VOID
FastbootMenuWaitKey (
  VOID
  )
{
  EFI_INPUT_KEY  Key;

  for (;;) {
    FastbootServiceUsb ();
    if (FastbootMenuReadKey (&Key)) {
      return;
    }
    gBS->Stall (10000);
  }
}

STATIC
VOID
FastbootMenuViewLog (
  VOID
  )
{
  UINTN          Start;
  UINTN          End;
  UINTN          Cursor;
  UINTN          Column;
  CHAR8          Line[80];
  CHAR8          Ch;
  EFI_INPUT_KEY  Key;

  if (gST->ConOut == NULL) {
    return;
  }
  FastbootStageLogs ();
  mMenuVisible = TRUE;
  mMenuDirty = TRUE;
  if (mStagedLength == 0) {
    Print (L"\fDarwinBoot fastboot log\n\nNo log bytes staged yet.\n\nPower: back\n");
    FastbootMenuWaitKey ();
    return;
  }
  Start = (mStagedLength > FASTBOOT_MENU_LOG_WINDOW) ?
          (mStagedLength - FASTBOOT_MENU_LOG_WINDOW) : 0;
  for (;;) {
    End = MIN (Start + FASTBOOT_MENU_LOG_WINDOW, mStagedLength);
    Print (L"\fDarwinBoot fastboot log  %lu..%lu of %lu\n\n",
           (UINT64)Start, (UINT64)End, (UINT64)mStagedLength);
    ZeroMem (Line, sizeof (Line));
    Column = 0;
    for (Cursor = Start; Cursor < End; Cursor++) {
      Ch = (CHAR8)mStaged[Cursor];
      if (Ch == '\r') {
        continue;
      }
      if (Ch == '\n') {
        Line[Column] = 0;
        Print (L"%a\n", Line);
        Column = 0;
        continue;
      }
      if ((Ch < 0x20) || (Ch > 0x7E)) {
        Ch = '.';
      }
      Line[Column++] = Ch;
      if (Column == sizeof (Line) - 1) {
        Line[Column] = 0;
        Print (L"%a\n", Line);
        Column = 0;
      }
    }
    if (Column != 0) {
      Line[Column] = 0;
      Print (L"%a\n", Line);
    }
    Print (L"\n  Volume up: older   Volume down: newer   Power: back\n");
    for (;;) {
      FastbootServiceUsb ();
      if (!FastbootMenuReadKey (&Key)) {
        gBS->Stall (10000);
        continue;
      }
      if (Key.ScanCode == SCAN_UP) {
        if (Start != 0) {
          Start = (Start > FASTBOOT_MENU_LOG_WINDOW) ?
                  (Start - FASTBOOT_MENU_LOG_WINDOW) : 0;
        }
        break;
      }
      if (Key.ScanCode == SCAN_DOWN) {
        if (End < mStagedLength) {
          Start = MIN (Start + FASTBOOT_MENU_LOG_WINDOW, mStagedLength - 1);
        }
        break;
      }
      if (FastbootMenuConfirmKey (&Key)) {
        return;
      }
    }
  }
}

STATIC
VOID
FastbootMenuActivate (
  VOID
  )
{
  EFI_STATUS  Status;

  switch (mMenuSelection) {
    case 0:
      if (mBusy || mStarted) {
        mMenuDirty = TRUE;
        return;
      }
      mMenuVisible = FALSE;
      mMenuDirty = FALSE;
      Status = FastbootStartKernel (0);
      if (EFI_ERROR (Status)) {
        mMenuVisible = TRUE;
        mMenuDirty = TRUE;
        if (gST->ConOut != NULL) {
          Print (L"DARWINBOOT: kernel start failed: %r\n", Status);
        }
      }
      break;
    case 1:
      FastbootMenuViewLog ();
      break;
    case 2:
      if (EFI_ERROR (FastbootStopKernel ())) {
        if (gST->ConOut != NULL) {
          Print (L"DARWINBOOT: the kernel is not running\n");
        }
      } else if (gST->ConOut != NULL) {
        Print (L"DARWINBOOT: stop requested\n");
      }
      mMenuDirty = TRUE;
      break;
    case 3:
      FastbootLog ("restart requested\n");
      if (gST->ConOut != NULL) {
        Print (L"DARWINBOOT: restarting\n");
      }
      gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
      if (gST->ConOut != NULL) {
        Print (L"DARWINBOOT: reset is not supported\n");
      }
      mMenuDirty = TRUE;
      break;
    case 4:
      FastbootLog ("shutdown requested\n");
      if (gST->ConOut != NULL) {
        Print (L"DARWINBOOT: shutting down\n");
      }
      gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
      if (gST->ConOut != NULL) {
        Print (L"DARWINBOOT: shutdown is not supported\n");
      }
      mMenuDirty = TRUE;
      break;
    default:
      break;
  }
}

STATIC
VOID
FastbootMenuHandleKey (
  VOID
  )
{
  EFI_INPUT_KEY  Key;

  if (!FastbootMenuReadKey (&Key)) {
    return;
  }
  if (!mMenuVisible) {
    mMenuVisible = TRUE;
    mMenuDirty = TRUE;
    return;
  }
  if (Key.ScanCode == SCAN_UP) {
    FastbootMenuMove (-1);
  } else if (Key.ScanCode == SCAN_DOWN) {
    FastbootMenuMove (1);
  } else if (FastbootMenuConfirmKey (&Key)) {
    FastbootMenuActivate ();
  }
}

STATIC
VOID
FastbootMenuDraw (
  VOID
  )
{
  UINTN    Index;
  BOOLEAN  Selected;

  if ((gST->ConOut == NULL) || !mMenuVisible) {
    return;
  }
  if (!FastbootMenuEnabled (mMenuSelection)) {
    FastbootMenuMove (1);
  }
  Print (L"\f");
  Print (L"UniXNU DarwinBoot  fastboot menu\n\n");
  if (mUsb == NULL) {
    Print (L"  USB      unavailable\n");
  } else {
    Print (L"  USB      %s\n", mUsbConnected ? L"host connected" : L"waiting for host");
  }
  if (mBusy) {
    Print (L"  kernel   running, checkpoint %lu\n", mCheckpoint);
  } else if (mStarted) {
    Print (L"  kernel   %a\n", (mResult[0] != 0) ? mResult : "stopped");
  } else {
    Print (L"  kernel   not started\n");
  }
  Print (L"  log      %lu bytes staged\n\n", (UINT64)mStagedLength);
  for (Index = 0; Index < FASTBOOT_MENU_ENTRIES; Index++) {
    Selected = (BOOLEAN)(Index == mMenuSelection);
    Print (L"  %c %u. %s%a\n",
           Selected ? L'>' : L' ',
           (UINT32)(Index + 1),
           mMenuTitles[Index],
           FastbootMenuEnabled (Index) ? "" : "  (unavailable)");
  }
  Print (L"\n");
  if (mMenuInput) {
    Print (L"  Volume up/down: select    Power: choose\n");
  } else {
    Print (L"  No key input: use the fastboot commands below\n");
  }
  Print (L"  fastboot oem run | oem log | oem stop | get_staged\n");
}

STATIC
VOID
EFIAPI
FastbootUsbInitNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  (VOID)Event;
  (VOID)Context;
}

STATIC
EFI_STATUS
FastbootUsbInit (
  VOID
  )
{
  EFI_STATUS  Status;

  mUsb = NULL;
  mUsbConnected = FALSE;
  mUsbReading = FALSE;
  FastbootLog ("USB init start\n");
  Print (L"DARWINBOOT: USB init start\n");
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  FastbootUsbInitNotify,
                  NULL,
                  &mQcomUsbInitGuid,
                  &mUsbInitEvent
                  );
  if (!EFI_ERROR (Status)) {
    gBS->SignalEvent (mUsbInitEvent);
    gBS->CloseEvent (mUsbInitEvent);
    mUsbInitEvent = NULL;
    FastbootLog ("USB init event ok\n");
    Print (L"DARWINBOOT: USB init event ok\n");
  } else {
    FastbootLog ("USB init event failed: %r\n", Status);
    Print (L"DARWINBOOT: USB init event failed: %r\n", Status);
  }
  Status = gBS->LocateProtocol (&mQcomUsbGuid, NULL, (VOID **)&mUsb);
  if (EFI_ERROR (Status)) {
    FastbootLog ("Qcom USB protocol unavailable: %r\n", Status);
    Print (L"DARWINBOOT: Qcom USB protocol unavailable: %r\n", Status);
    return Status;
  }
  FastbootLog ("USB protocol 0x%lx rev 0x%lx\n",
               (UINT64)(UINTN)mUsb, mUsb->Revision);
  Print (L"DARWINBOOT: USB protocol 0x%lx rev 0x%lx\n",
         (UINT64)(UINTN)mUsb, mUsb->Revision);
  Status = mUsb->StartEx (&mDescriptors);
  if (EFI_ERROR (Status)) {
    FastbootLog ("USB start_ex failed: %r\n", Status);
    Print (L"DARWINBOOT: USB start_ex failed: %r\n", Status);
    return Status;
  }
  FastbootLog ("USB start_ex ok\n");
  Print (L"DARWINBOOT: USB start_ex ok\n");
  Status = mUsb->AllocateTransferBuffer (USB_TRANSFER_SIZE, &mUsbRx);
  if (EFI_ERROR (Status)) {
    FastbootLog ("USB rx buffer failed: %r\n", Status);
    Print (L"DARWINBOOT: USB rx buffer failed: %r\n", Status);
    return Status;
  }
  Status = mUsb->AllocateTransferBuffer (USB_TRANSFER_SIZE, &mUsbTx);
  if (EFI_ERROR (Status)) {
    FastbootLog ("USB tx buffer failed: %r\n", Status);
    Print (L"DARWINBOOT: USB tx buffer failed: %r\n", Status);
    return Status;
  }
  mUsbConnected = TRUE;
  FastbootLog ("USB fastboot ready (rx 0x%lx tx 0x%lx)\n",
               (UINT64)(UINTN)mUsbRx, (UINT64)(UINTN)mUsbTx);
  Print (L"DARWINBOOT: USB fastboot ready\n");
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
DarwinFastbootMode (
  IN UINT64       ArgsAddress,
  IN UINT64       EntryAddress,
  IN UINT64       MemoryBase,
  IN UINT64       MemorySize,
  IN UINT64       KernelBase,
  IN UINT64       KernelSize,
  IN UINT64       Video[6],
  IN CONST CHAR8  *CommandLine
  )
{
  DARWIN_BOOT_ARGS  *Args;
  EFI_STATUS        Status;

  mArgsAddress = ArgsAddress;
  mEntryAddress = EntryAddress;
  mMemoryBase = MemoryBase;
  mMemorySize = MemorySize;
  mKernelBase = KernelBase;
  mKernelSize = KernelSize;
  mKernelSlide = MemoryBase - KernelBase;
  mKernelLivePa = 0;
  mBusy = FALSE;
  mStarted = FALSE;
  mCheckpoint = 0;
  mResult[0] = 0;
  CopyMem (mVideo, Video, sizeof (mVideo));
  Args = (DARWIN_BOOT_ARGS *)(UINTN)ArgsAddress;
  if (!FastbootContains (Args->CommandLine, "cpus=")) {
    AsciiStrnCatS (Args->CommandLine, sizeof (Args->CommandLine), " cpus=1", 7);
  }
  if (gST->ConOut != NULL) {
    Print (L"DARWINBOOT: fastboot mode; resident UEFI, secondary-core XNU\n");
  }
  FastbootLog ("fastboot mode args=0x%lx entry=0x%lx kernel=0x%lx+0x%lx slide=0x%lx\n",
               ArgsAddress, EntryAddress, KernelBase, KernelSize, mKernelSlide);
  FastbootLog ("command line: %a\n", Args->CommandLine);
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  mMenuInput = FastbootMenuInputAvailable ();
  mMenuSelection = 0;
  mMenuVisible = TRUE;
  mMenuDirty = TRUE;
  Status = FastbootUsbInit ();
  if (EFI_ERROR (Status)) {
    mUsb = NULL;
    if (!mMenuInput) {
      return Status;
    }
    if (gST->ConOut != NULL) {
      Print (L"DARWINBOOT: USB unavailable; the boot menu stays available\n");
    }
  }
  FastbootLog ("entering menu and fastboot loop (input %a)\n",
               mMenuInput ? "yes" : "no");
  FastbootPatchKernel (0, FALSE);
  mMenuDirty = TRUE;
  for (;;) {
    FastbootServiceUsb ();
    FastbootMenuHandleKey ();
    if (mMenuVisible && mMenuDirty) {
      FastbootMenuDraw ();
      mMenuDirty = FALSE;
    }
    gBS->Stall (mBusy ? 2000 : 10000);
  }
  return EFI_ABORTED;
}
