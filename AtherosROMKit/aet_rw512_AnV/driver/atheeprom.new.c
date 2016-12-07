#include <NTDDK.h>

#define IO_ERR_MAP 0x901
#define IO_ERR_LOWMEM 0x902

#define AR_SREV_9300_20_OR_LATER(ah)			0
#define AR_GPIO_OE_OUT                          (AR_SREV_9300_20_OR_LATER(ah) ? 0x4050 : 0x404c)
#define AR_GPIO_IN_OUT                           0x4048
#define AR_GPIO_OUTPUT_MUX1						(AR_SREV_9300_20_OR_LATER(ah) ? 0x4068 : 0x4060)
#define AR_GPIO_OUTPUT_MUX2                     (AR_SREV_9300_20_OR_LATER(ah) ? 0x406c : 0x4064)
#define AR_GPIO_OUTPUT_MUX3                     (AR_SREV_9300_20_OR_LATER(ah) ? 0x4070 : 0x4068)

#define AR_INPUT_STATE                           (AR_SREV_9300_20_OR_LATER(ah) ? 0x4074 : 0x406c)

#define AR_EEPROM_STATUS_DATA                    (AR_SREV_9300_20_OR_LATER(ah) ? 0x4084 : 0x407c)
#define AR_EEPROM_STATUS_DATA_VAL                0x0000ffff
#define AR_EEPROM_STATUS_DATA_VAL_S              0
#define AR_EEPROM_STATUS_DATA_BUSY               0x00010000
#define AR_EEPROM_STATUS_DATA_BUSY_ACCESS        0x00020000
#define AR_EEPROM_STATUS_DATA_PROT_ACCESS        0x00040000
#define AR_EEPROM_STATUS_DATA_ABSENT_ACCESS      0x00080000

#define AR5416_EEPROM_MAGIC 0xa55a
#define AR5416_EEPROM_MAGIC_OFFSET  0x0
#define AR5416_EEPROM_S             2
#define AR5416_EEPROM_OFFSET        0x2000
#define AR5416_EEPROM_MAX           0xae0


NTSTATUS eepromWait (char* ioMem)
{
	unsigned i;

	for (i = 1000; i; i--)
	{
		unsigned regStatus = *(volatile unsigned int*)(ioMem + AR_EEPROM_STATUS_DATA);

		if (0 == (regStatus & 0xFFFF0000))
			return STATUS_SUCCESS;

		KeStallExecutionProcessor(100);
	}

	return STATUS_INTERNAL_ERROR;
}

NTSTATUS eepromReadWord (char* ioMem, unsigned int wordNum, unsigned short* rb)
{
	unsigned int offset = AR5416_EEPROM_OFFSET + (wordNum << AR5416_EEPROM_S);
	unsigned long eeDword = *(volatile unsigned long*)(ioMem + offset);

	if (eepromWait(ioMem) != STATUS_SUCCESS)
		return STATUS_IO_DEVICE_ERROR;

	*rb = AR_EEPROM_STATUS_DATA_VAL & *(volatile unsigned long*)(ioMem + AR_EEPROM_STATUS_DATA);
	return STATUS_SUCCESS;
}

NTSTATUS eepromWriteWord (char* ioMem, unsigned int wordNum, short word)
{
	unsigned int offset = AR5416_EEPROM_OFFSET + (wordNum << AR5416_EEPROM_S);
	unsigned long eeDword;
	unsigned long j;

	eeDword = *(volatile unsigned long*)(ioMem + AR_GPIO_OUTPUT_MUX1);
	*(volatile unsigned long*)(ioMem + AR_GPIO_OUTPUT_MUX1) = eeDword & 0xFFF07FFF;
	KeStallExecutionProcessor(1);
	
	eeDword = *(volatile unsigned long*)(ioMem + AR_GPIO_OE_OUT);
	*(volatile unsigned long*)(ioMem + AR_GPIO_OE_OUT) = eeDword | 0xC0;
	KeStallExecutionProcessor(1);
	
	eeDword = *(volatile unsigned long*)(ioMem + AR_GPIO_IN_OUT);
	*(volatile unsigned long*)(ioMem + AR_GPIO_IN_OUT) = eeDword & ~8;
	KeStallExecutionProcessor(1);
	
	*(volatile unsigned short*)(ioMem + offset) = word;

	for (j = 50000; j; j--)
	{
	  eeDword = *(volatile unsigned long*)(ioMem + AR_EEPROM_STATUS_DATA);
	  if ( 0 == (eeDword & 0xF0000) )
	  {
		eeDword = *(volatile unsigned long*)(ioMem + AR_GPIO_IN_OUT);
		*(volatile unsigned long*)(ioMem + AR_GPIO_IN_OUT) = eeDword | 8;
		return STATUS_SUCCESS;
	  }
	  KeStallExecutionProcessor(1);
	}

	return STATUS_IO_DEVICE_ERROR;
}


#define IOCTL_EEPROM_Read_Function CTL_CODE(0x89A4, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_EEPROM_Write_Function CTL_CODE(0x89A4, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_EEPROM_Get_Size_Function CTL_CODE(0x89A4, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)


OBJECT_ATTRIBUTES objAttributes;

HANDLE hFile;
IO_STATUS_BLOCK    ioStatusBlock;
//UNICODE_STRING     uniName;
UNICODE_STRING     devName;
UNICODE_STRING     linkName;
PDEVICE_OBJECT     devObj;

DRIVER_UNLOAD Unload;

VOID 
  Unload(
    __in struct _DRIVER_OBJECT  *DriverObject
    )
  {
	  IoDeleteSymbolicLink(&linkName);
	  IoDeleteDevice(devObj);
	  return;
}

DRIVER_DISPATCH DispatchCreateClose;
NTSTATUS
  DispatchCreateClose(
    __inout struct _DEVICE_OBJECT  *DeviceObject,
    __in struct _IRP  *Irp 
    )
  {
    NTSTATUS NtStatus = STATUS_SUCCESS;
    Irp->IoStatus.Status = NtStatus;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return NtStatus;
}

DRIVER_DISPATCH DispatchDeviceControl;

NTSTATUS
  DispatchDeviceControl(
    __inout struct _DEVICE_OBJECT  *DeviceObject,
    __in struct _IRP  *Irp 
    )
{
    NTSTATUS NtStatus = STATUS_NOT_SUPPORTED;
    unsigned long dwDataWritten = 0;

    PIO_STACK_LOCATION pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);

    if(pIoStackIrp) /* Should Never Be NULL! */
    {
		unsigned long inBufLen = pIoStackIrp->Parameters.DeviceIoControl.InputBufferLength;
		unsigned long outBufLen = pIoStackIrp->Parameters.DeviceIoControl.OutputBufferLength;

		PHYSICAL_ADDRESS athPhyAddr = { 0, 0 };
		char* mappedIo = NULL;

		unsigned long eepStartLoc = 0;
		unsigned short* eeprom = NULL;
		unsigned short eepromWord = 0;
		int i;

        switch(pIoStackIrp->Parameters.DeviceIoControl.IoControlCode)
        {
            case IOCTL_EEPROM_Read_Function:

				if (outBufLen == 376) eepStartLoc = 64;
				else if (outBufLen == 512) eepStartLoc = 0;
				else if (outBufLen == 727) eepStartLoc = 128;
				else if (outBufLen == 3256) eepStartLoc = 0x100;
				else {
					NtStatus = STATUS_INVALID_PARAMETER;
					break;
				}

				// map device address space
				athPhyAddr = *(PHYSICAL_ADDRESS*)Irp->AssociatedIrp.SystemBuffer;
				mappedIo = (char*)MmMapIoSpace(athPhyAddr, 0x10000, MmNonCached);
				if (mappedIo == NULL) {
					NtStatus = STATUS_INTERNAL_ERROR;
					break;
				}

				// produce output
				eeprom = Irp->AssociatedIrp.SystemBuffer;
				if (eepromReadWord(mappedIo, eepStartLoc, &eeprom[0]) == STATUS_SUCCESS && eeprom[0] == (eepStartLoc == 0 ? AR5416_EEPROM_MAGIC : outBufLen))
				{
					NtStatus = STATUS_SUCCESS;
					for (i = 1; i < outBufLen/2; i++)
					{
						NtStatus = eepromReadWord(mappedIo, eepStartLoc + i, &eeprom[i]);
						if (NtStatus != STATUS_SUCCESS)
							break;
					}
					if (NtStatus == STATUS_SUCCESS)
						dwDataWritten = outBufLen;
				}
				else
					NtStatus = STATUS_INTERNAL_ERROR;

				MmUnmapIoSpace(mappedIo,0x10000);
                break;

			case IOCTL_EEPROM_Get_Size_Function:
				if (outBufLen != 4) {
					NtStatus = STATUS_INVALID_PARAMETER;
					break;
				}

				// map device address space
				athPhyAddr = *(PHYSICAL_ADDRESS*)Irp->AssociatedIrp.SystemBuffer;
				mappedIo = (char*)MmMapIoSpace(athPhyAddr, 0x10000, MmNonCached);
				if (mappedIo == NULL) {
					NtStatus = STATUS_INTERNAL_ERROR;
					break;
				}

				// produce output
				if(eepromReadWord(mappedIo, 64, &eepromWord) == STATUS_SUCCESS && eepromWord == 376
				|| eepromReadWord(mappedIo, 128, &eepromWord) == STATUS_SUCCESS && eepromWord == 727
				|| eepromReadWord(mappedIo, 0x100, &eepromWord) == STATUS_SUCCESS && eepromWord == 3256)
				{
					*(unsigned long*)Irp->AssociatedIrp.SystemBuffer = eepromWord;
					dwDataWritten = 4;
					NtStatus = STATUS_SUCCESS;
				}
				else
				{
					*(unsigned long*)Irp->AssociatedIrp.SystemBuffer = 0;
					dwDataWritten = 0;
					NtStatus = STATUS_INTERNAL_ERROR;
				}

				MmUnmapIoSpace(mappedIo,0x10000);   
                break;

			case IOCTL_EEPROM_Write_Function:

				if (inBufLen == 376 + 8) eepStartLoc = 64;
				else if (inBufLen == 512 + 8) eepStartLoc = 0;
				else if (inBufLen == 727 + 8) eepStartLoc = 128;
				else if (inBufLen == 3256 + 8) eepStartLoc = 0x100;
				else {
					NtStatus = STATUS_INVALID_PARAMETER;
					break;
				}

				// map device address space
				athPhyAddr = *(PHYSICAL_ADDRESS*)Irp->AssociatedIrp.SystemBuffer;
				mappedIo = (char*)MmMapIoSpace(athPhyAddr, 0x10000, MmNonCached);
				if (mappedIo == NULL) {
					NtStatus = STATUS_INTERNAL_ERROR;
					break;
				}

				if (eepromReadWord(mappedIo, eepStartLoc, &eepromWord) != STATUS_SUCCESS 
				|| eepromWord != (eepStartLoc == 0 ? AR5416_EEPROM_MAGIC : inBufLen - 8))
				{
					MmUnmapIoSpace(mappedIo,0x10000);
					NtStatus = STATUS_INTERNAL_ERROR;
					break;
				}

				eeprom = 8 + (char*)Irp->AssociatedIrp.SystemBuffer;
				for (i = 0; i < (inBufLen-8)/2; i++)
				{
					NtStatus = eepromWriteWord(mappedIo, eepStartLoc + i, eeprom[i]);
					if (NtStatus == STATUS_SUCCESS) continue;
					else if (i == 0) NtStatus = STATUS_INTERNAL_ERROR;
					break;
				}

				MmUnmapIoSpace(mappedIo,0x10000);
				dwDataWritten = 0;
                break;
        }
    }

    Irp->IoStatus.Status = NtStatus;
    Irp->IoStatus.Information = dwDataWritten;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return NtStatus;
}

NTSTATUS 
  DriverEntry( 
    IN PDRIVER_OBJECT   DriverObject, 
    IN PUNICODE_STRING  RegistryPath
    )
{
	NTSTATUS NtStatus;

	RtlInitUnicodeString(&devName, L"\\Device\\atheeprom");
	NtStatus = IoCreateDevice(DriverObject, 0, &devName, 0x89A4, 0x100, 0, &devObj);

	if (NtStatus != STATUS_SUCCESS) return NtStatus;

	DriverObject->DriverUnload = &Unload;
	DriverObject->MajorFunction[IRP_MJ_CREATE]  = &DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]  = &DispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = &DispatchDeviceControl;

	RtlInitUnicodeString(&linkName, L"\\DosDevices\\atheeprom");
	NtStatus = IoCreateSymbolicLink(&linkName, &devName);

	if (NtStatus != STATUS_SUCCESS){
		IoDeleteDevice(devObj);
		return NtStatus;
	}
/*
	mappedIo = (void*)MmMapIoSpace(athPhyAddr,0x10000,MmNonCached);

	if (mappedIo == 0) return STATUS_DEVICE_CONFIGURATION_ERROR;

	for (i = 0; i < 0xBC; i++){
		eeDword = *(unsigned long*)(mappedIo+(i+0x40)*4+0x2000);
		if (eeDword){
			KeStallExecutionProcessor(1000);
			*(unsigned short*)(eeprom+i*2) = *(unsigned short*)(mappedIo+0x407C);
		}
	}

	MmUnmapIoSpace(mappedIo,0x10000);

	RtlInitUnicodeString(&uniName, L"\\??\\C:\\eeprom.bin");
	InitializeObjectAttributes(&objAttributes, &uniName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	ntstatus = ZwCreateFile(&hFile, GENERIC_READ | GENERIC_WRITE, &objAttributes, &ioStatusBlock, 0, FILE_ATTRIBUTE_NORMAL, 0, FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT, 0, 0);
	if (ntstatus == STATUS_SUCCESS){
		ZwWriteFile(hFile, 0, 0, 0, &ioStatusBlock, eeprom, 376, 0, 0);
		ZwClose(hFile);
	}
*/
    return STATUS_SUCCESS;
}