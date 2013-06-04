#include "ntddk.h"

#include "dbgmsg.h"
#include "ctrlcode.h"
#include "device.h"


/* MSNetDigaDeviceObject�������Ǵ������豸 */
PDEVICE_OBJECT MSNetDiagDeviceObject;
/* DriverObjectRef��������ע������� */
PDRIVER_OBJECT DriverObjectRef;


void TestCommand(PVOID inputBuffer, PVOID outputBuffer, ULONG inputBufferLength, ULONG outputBufferLength);
NTSTATUS defaultDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS dispatchIOControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);


NTSTATUS RegisterDriverDeviceName(IN PDRIVER_OBJECT DriverObject);
NTSTATUS RegisterDriverDeviceLink();

VOID Unload(IN PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT pdeviceObj;
	UNICODE_STRING unicodeString;
	DBG_TRACE("OnUnload","Received signal to unload the driver");
	pdeviceObj = (*DriverObject).DeviceObject;
	if(pdeviceObj != NULL)
	{
		DBG_TRACE("OnUnload","Unregistering driver's symbolic link");
		RtlInitUnicodeString(&unicodeString, DeviceLinkBuffer);
		IoDeleteSymbolicLink(&unicodeString);
		DBG_TRACE("OnUnload","Unregistering driver's device name");
		IoDeleteDevice((*DriverObject).DeviceObject);
	}
	return ;
}
/* 
 * DriverObject�൱��ע���������DeviceObjectΪ��Ӧĳ�������豸
 * һ���������Դ�������豸��Ȼ��ͨ��DriverObject::DeviceObject��
 * DeviceObject::NextDevice���������豸����
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	int i;
	NTSTATUS ntStatus;
	DBG_TRACE("Driver Entry","Driver has benn loaded");
	for(i=0;i<IRP_MJ_MAXIMUM_FUNCTION;i++)
	{
		(*DriverObject).MajorFunction[i] = defaultDispatch;
	}
	(*DriverObject).MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatchIOControl;
	(*DriverObject).DriverUnload = Unload;

	DBG_TRACE("Driver Entry","Registering driver's device name");
	ntStatus = RegisterDriverDeviceName(DriverObject);
	if(!NT_SUCCESS(ntStatus))
	{
		DBG_TRACE("Driver Entry","Failed to create device");
		return ntStatus;
	}

	DBG_TRACE("Driver Entry","Registering driver's symbolic link");
	if(!NT_SUCCESS(ntStatus))
	{
		DBG_TRACE("Driver Entry","Failed to create symbolic link");
		return ntStatus;
	}
	DriverObjectRef = DriverObject;
	return STATUS_SUCCESS;
}
/*
 * IRP.IoStatus : ����ΪIO_STATUS_BLOCK
 * A driver sets an IRP's I/O status block to indicate the final status of 
 * an I/O request, before calling IoCompleteRequest for the IRP.
 typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID    Pointer;
  };
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK

  Status: This is the completion status, either STATUS_SUCCESS if the 
          requested operation was completed successfully or an informational, 
          warning, or error STATUS_XXX value. 
  Information: This is set to a request-dependent value. For example, 
          on successful completion of a transfer request, this is set 
		  to the number of bytes transferred. If a transfer request is 
		  completed with another STATUS_XXX, this member is set to zero.

 */
NTSTATUS defaultDispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP IRP)
{
	((*IRP).IoStatus).Status = STATUS_SUCCESS;
	((*IRP).IoStatus).Information = 0;
	/*
	 The IoCompleteRequest routine indicates that the caller has 
	 completed all processing for a given I/O request and is 
	 returning the given IRP to the I/O manager.
	 */
	IoCompleteRequest(IRP, IO_NO_INCREMENT);
	return (STATUS_SUCCESS);
}
/*
 * I/O��ջ��Ԫ��IO_STACK_LOCATION���壬ÿһ����ջ��Ԫ����Ӧһ���豸����
 * ����֪������һ�����������У����Դ���һ�������豸���󣬶���Щ�豸����
 * ����Ӧ��һ��IO_STACK_LOCATION�ṹ�壬�������������еĶ���豸���󣬶�
 * ��Щ�豸����֮��Ĺ�ϵΪˮƽ��ι�ϵ��
 * Parameters Ϊÿ�����͵� request �ṩ���������磺Create(IRP_MJ_CREATE ���󣩣�
 * Read��IRP_MJ_READ ���󣩣�StartDevice��IRP_MJ_PNP ������ IRP_MN_START_DEVICE��
 * 
	//
	// NtDeviceIoControlFile ����
	//
	struct
	{
		ULONG OutputBufferLength;
		ULONG POINTER_ALIGNMENT InputBufferLength;
		ULONG POINTER_ALIGNMENT IoControlCode;
		PVOID Type3InputBuffer;
	} DeviceIoControl;
	��DriverEntry�����У���������dispatchIOControl����IRP_MJ_DEVICE_CONTROL
	���͵����������dispatchIOControl�У�����ֻ����IOCTL����Parameters��
	ֻ����DeviceIoControl��Ա
 */
NTSTATUS dispatchIOControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP IRP)
{
	PIO_STACK_LOCATION irpStack;
	PVOID inputBuffer;
	PVOID outputBuffer;
	ULONG inBufferLength;
	ULONG outBufferLength;
	ULONG ioctrlcode;
	NTSTATUS ntStatus;
	ntStatus = STATUS_SUCCESS;
	((*IRP).IoStatus).Status = STATUS_SUCCESS;
	((*IRP).IoStatus).Information = 0;
	inputBuffer = (*IRP).AssociatedIrp.SystemBuffer;
	outputBuffer = (*IRP).AssociatedIrp.SystemBuffer;
	irpStack = IoGetCurrentIrpStackLocation(IRP);
	inBufferLength = (*irpStack).Parameters.DeviceIoControl.InputBufferLength;
	outBufferLength = (*irpStack).Parameters.DeviceIoControl.OutputBufferLength;
	ioctrlcode = (*irpStack).Parameters.DeviceIoControl.IoControlCode;

	DBG_TRACE("dispatchIOControl","Received a command");
	switch(ioctrlcode)
	{
	case IOCTL_TEST_CMD:
		{
			TestCommand(inputBuffer, outputBuffer, inBufferLength, outBufferLength);
			((*IRP).IoStatus).Information = outBufferLength;
		}
		break;
	default:
		{
			DBG_TRACE("dispatchIOControl","control code not recognized");
		}
		break;
	}
	/* �ڴ���������󣬵���IoCompleteRequest */
	IoCompleteRequest(IRP, IO_NO_INCREMENT);
	return(ntStatus);
}

void TestCommand(PVOID inputBuffer, PVOID outputBuffer, ULONG inputBufferLength, ULONG outputBufferLength)
{
	char *ptrBuffer;
	DBG_TRACE("dispathIOControl","Displaying inputBuffer");
	ptrBuffer = (char*)inputBuffer;
	DBG_PRINT2("[dispatchIOControl]: inputBuffer=%s\n", ptrBuffer);
	DBG_TRACE("dispatchIOControl","Populating outputBuffer");
	ptrBuffer = (char*)outputBuffer;
	ptrBuffer[0] = '!';
	ptrBuffer[1] = '1';
	ptrBuffer[2] = '2';
	ptrBuffer[3] = '3';
	ptrBuffer[4] = '!';
	ptrBuffer[5] = '\0';
	DBG_PRINT2("[dispatchIOControl]:outputBuffer=%s\n", ptrBuffer);
	return;
}

NTSTATUS RegisterDriverDeviceName(IN PDRIVER_OBJECT DriverObject)
{
	NTSTATUS ntStatus;
	UNICODE_STRING unicodeString;
	/* ����DeviceNameBuffer����ʼ��unicodeString */
	RtlInitUnicodeString(&unicodeString, DeviceNameBuffer);
	/*
	 * ����һ���豸���豸����ΪFILE_DEVICE_RK���������Լ���device.h�ж���)��
	 * �������豸������MSNetDiagDeviceObject��
	 */
	ntStatus = IoCreateDevice
		(
		    DriverObject,
			0,
			&unicodeString,
			FILE_DEVICE_RK,
			0,
			TRUE,&MSNetDiagDeviceObject
		);
	return (ntStatus);
}

NTSTATUS RegisterDriverDeviceLink()
{
	NTSTATUS ntStatus;
	UNICODE_STRING unicodeString;
	UNICODE_STRING unicodeLinkString;
	RtlInitUnicodeString(&unicodeString, DeviceNameBuffer);
	RtlInitUnicodeString(&unicodeString, DeviceLinkBuffer);
	/*
	 * IoCreateSymbolicLink����һ���豸���ӡ�������������Ȼע�����豸��
	 * ����ֻ�����ں��пɼ���Ϊ��ʹӦ�ó���ɼ���������Ӵ����¶һ������
	 * ���ӣ�������ָ���������豸��
	 */
	ntStatus = IoCreateSymbolicLink(&unicodeLinkString, &unicodeString);
	return (ntStatus);
}