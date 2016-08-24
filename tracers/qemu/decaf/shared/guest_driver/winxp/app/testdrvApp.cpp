//
// testdrvApp.cpp  : Defines the entry point for the application.
//
// Generated by C DriverWizard 3.2.0 (Build 2485)
// Requires DDK Only
// File created on 11/15/2005
//
#include "testdrvApp.h"

// Array of possible transfer types
PTCHAR g_TransferTypeArray[] =
{
    _T("ReadFile"),
    _T("WriteFile"),
    _T("")
};

HANDLE              g_hDevice = INVALID_HANDLE_VALUE;
HDEVNOTIFY          g_hInterfaceNotification = NULL;
TESTDRV_LIST_ITEM   g_IoList;
CRITICAL_SECTION    g_IoListLock;
HANDLE              g_hIoCompletionThreadTerminationEvent;
HANDLE              g_hIoCompletionThread;

///////////////////////////////////////////////////////////////////////////////////////////////////
//  testdrvOutputText
//      method to output text in the output window
//
//  Arguments:
//      IN  Format
//              Text format to print to output window
//
//  Return Value:
//      None.
//
VOID testdrvOutputText(LPCTSTR Format, ...)
{
    TCHAR       str[MAX_STRING_LENGTH];
    va_list     vaList;

    va_start(vaList, Format);

    _vstprintf(str, Format, vaList);

    OutputDebugString(_T("testdrv: "));
    OutputDebugString(str);
    OutputDebugString(_T("\n"));

    va_end(vaList);

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  testdrvOutputBuffer
//      method to output text in the output window
//
//  Arguments:
//      IN  Buffer
//              Data Buffer
//
//      IN  Size
//              Size of Data Buffer
//
//  Return Value:
//      None.
//
VOID testdrvOutputBuffer(PVOID Buffer, ULONG Size)
{
    TCHAR       str[MAX_STRING_LENGTH];
    LONG        length = (LONG)Size;
    PUCHAR      p = (PUCHAR)Buffer;
    TCHAR       data[MAX_STRING_LENGTH];
    TCHAR       rawData[MAX_STRING_LENGTH];
    ULONG       i;
    ULONG       j;

    for (i = 0; i < Size; i += 16)
    {
        ZeroMemory(str, sizeof(str));
        ZeroMemory(rawData, sizeof(rawData));

        _stprintf(str, _T("%04.4X  "), i);

        for (j = 0; j < 16; ++j, ++p)
        {
            if (length > 0)
            {
                _stprintf(data, _T("%02X "), *p);
            }
            else
            {
                _stprintf(data, _T("   "));
            }

            _tcsncat(str, data, 3);

            if (length > 0)
            {
                TCHAR c = (TCHAR)(*p);

                if (_istalnum(_TUCHAR(c)))
                {
                    _stprintf(data, _T("%c"), c);
                }
                else
                {
                    _tcsncat(rawData, _T("?"), 1);
                }

                --length;
            }
        }

        _tcsncat(str, _T("  "), 2);
        _tcsncat(str, rawData, 16);

        OutputDebugString(str);
        OutputDebugString(_T("\n"));
    }

    return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  testdrvEnumerateDevices
//      Finds devices associated with the given interface class
//
//  Arguments:
//      IN  hDlg
//              Handle to dialog
//
//  Return Value:
//      status.
//
DWORD testdrvEnumerateDevices(HWND hDlg)
{
    HWND                                hList;
    DWORD                               lastError;
    HANDLE                              hDev;

    // Get the handle to the device instance list box
    hList = GetDlgItem(hDlg, IDC_DEVICE_INSTANCE_LIST);
    if (hList == NULL)
    {
        lastError = GetLastError();
        testdrvOutputText(_T("GetDlgItem failed, GetLastError() = %d"), lastError);
        return lastError;
    }

    // Clear the instance list box
    SendMessage(hList, LB_RESETCONTENT, 0, 0);

    // Open handle to device
    hDev = CreateFile(
                 _T("\\\\.\\testdrvDevice"),
                 GENERIC_READ,
                 FILE_SHARE_READ,
                 NULL,
                 OPEN_EXISTING,
                 0,
                 0
                 );

    if (hDev != INVALID_HANDLE_VALUE)
    {
        SendMessage(
            hList,
            LB_ADDSTRING,
            0,
            (LPARAM)_T("\\\\.\\testdrvDevice")
            );

        CloseHandle(hDev);
    }
    else
    {
        testdrvOutputText(_T("No devices found"));
    }

    return ERROR_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  testdrvOpenDevice
//      Opens the nth device found with the given interface class
//
//  Arguments:
//      IN  hDlg
//              Handle to dialog
//
//  Return Value:
//      Handle to device.
//
HANDLE testdrvOpenDevice(HWND hDlg)
{
    HWND    hList;
    HANDLE  hDev;
    TCHAR   path[MAX_STRING_LENGTH];
    DWORD   itemIndex;

    // Get handle to device instance list box
    hList = GetDlgItem(hDlg, IDC_DEVICE_INSTANCE_LIST);

    // Find the current selection
    itemIndex = (DWORD)SendMessage(hList, LB_GETCURSEL, 0, 0);

    // Get the path string from the list box
    (LPTSTR)SendMessage(
                hList,
                LB_GETTEXT,
                itemIndex,
                (LPARAM)path);

    // Open handle to device
    hDev = CreateFile(
                 path,
                 GENERIC_READ | GENERIC_WRITE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 NULL,
                 OPEN_EXISTING,
                 FILE_FLAG_OVERLAPPED,
                 0
                 );

    if (hDev == INVALID_HANDLE_VALUE)
    {
        testdrvOutputText(_T("Error: CreateFile failed for device %s (%d)\n"), path, GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    testdrvOutputText(_T("Opened device %s"), path);

    // Reflect the selection in the current selection edit box
    SetDlgItemText(hDlg, IDC_SELECTED_DEVICE_EDIT, path);

    return hDev;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  testdrvMainDlgProc
//      Message loop
//
//  Arguments:
//      IN  hDlg
//              Handle to dialog
//
//      IN  uMsg
//              Message id
//
//      IN  wParam
//              WPARAM
//
//      IN  lParam
//              LPARAM
//
//  Return Value:
//      status.
//
LRESULT CALLBACK testdrvMainDlgProc(
    HWND    hDlg,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam
    )
{
    HWND    hWnd;
    TCHAR   str[MAX_STRING_LENGTH];
    DWORD   itemIndex;
    DWORD   error = ERROR_SUCCESS;
    DWORD   ii;
    BOOL    bInBufferEnable = TRUE;
    BOOL    bOutBufferEnable = TRUE;

    switch (uMsg)
    {
    case WM_INITDIALOG:

        // Initialize our device handle
        g_hDevice = INVALID_HANDLE_VALUE;

        // Populate the list box
        testdrvEnumerateDevices(hDlg);

        // Setup our operation combo box
        hWnd = GetDlgItem(hDlg, IDC_OP_TYPE_COMBO);

        for(ii = 0; _tcscmp(g_TransferTypeArray[ii], _T("")); ii++)
        {
            // Add the transfer type string to the combo box
            SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)g_TransferTypeArray[ii]);
        }

        // Initialize the window.
        return 1;

    case WM_COMMAND:

        switch (LOWORD(wParam))
        {

        case IDOK:
            EndDialog(hDlg, 0);
            PostQuitMessage(0);
            break;

        case IDC_OPEN_BUTTON:

            // Open the selected device
            g_hDevice = testdrvOpenDevice(hDlg);
            if (g_hDevice == INVALID_HANDLE_VALUE)
            {
                testdrvOutputText(_T("Failed to open device, all options will be unavailable"));
                return 0;
            }

            // If we opened the device, disable the open button, only one
            // open at a time in this simple app
            EnableWindow(GetDlgItem(hDlg, IDC_OPEN_BUTTON), FALSE);

            // Enable the close button
            EnableWindow(GetDlgItem(hDlg, IDC_CLOSE_BUTTON), TRUE);

            // Enable the operation combo box
            EnableWindow(GetDlgItem(hDlg, IDC_OPERATION_TYPE_STATIC), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_OP_TYPE_COMBO), TRUE);

            // Clean up window-specific data objects.
            return 0;

        case IDC_CLOSE_BUTTON:

            // Open the selected device
            if (!CloseHandle(g_hDevice))
            {
                testdrvOutputText(_T("Failed to close device"));
                return 0;
            }

            testdrvOutputText(_T("Device closed"));

            // Set our handle to an invalid state
            g_hDevice = INVALID_HANDLE_VALUE;

            // If we closed the device, disable the close button
            EnableWindow(GetDlgItem(hDlg, IDC_CLOSE_BUTTON), FALSE);

            // See if we have a current selection
            // Reflect the change in the current selection box
            itemIndex = (DWORD)SendMessage(
                            GetDlgItem(hDlg, IDC_DEVICE_INSTANCE_LIST),
                            LB_GETCURSEL,
                            0,
                            0
                            );

            // If we have a selection, enable the open button
            EnableWindow(GetDlgItem(hDlg, IDC_OPEN_BUTTON), TRUE);

            // Clear the selection in the current selection edit box
            SetDlgItemText(hDlg, IDC_SELECTED_DEVICE_EDIT, NULL);

            // Disable the operation combo box
            EnableWindow(GetDlgItem(hDlg, IDC_OPERATION_TYPE_STATIC), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_OP_TYPE_COMBO), FALSE);

            // Disable the input data pattern edit box
            EnableWindow(GetDlgItem(hDlg, IDC_IN_DATA_STATIC), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_IN_DATA_EDIT), FALSE);

            // Disable the output data pattern edit box
            EnableWindow(GetDlgItem(hDlg, IDC_OUT_DATA_STATIC), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_OUT_DATA_EDIT), FALSE);

            // Disable the input buffer size edit box
            EnableWindow(GetDlgItem(hDlg, IDC_IN_SIZE_STATIC), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_IN_SIZE_EDIT), FALSE);

            // Disable the output buffer size edit box
            EnableWindow(GetDlgItem(hDlg, IDC_OUT_SIZE_STATIC), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_OUT_SIZE_EDIT), FALSE);

            // Disable the execute button
            EnableWindow(GetDlgItem(hDlg, IDC_EXECUTE_BUTTON), FALSE);

            // Disable the cancel I/O button
            EnableWindow(GetDlgItem(hDlg, IDC_CANCEL_IO_BUTTON), FALSE);

            // remove current I/O selection
            SendMessage(GetDlgItem(hDlg, IDC_OP_TYPE_COMBO), CB_SETCURSEL, -1, 0);

            // Clean up window-specific data objects.
            return 0;

        case IDC_DEVICE_INSTANCE_LIST:
            switch (HIWORD(wParam))
            {
            case LBN_SELCHANGE:

                hWnd = (HWND)lParam;

                // Reflect the change in the current selection box
                itemIndex = (DWORD)SendMessage(hWnd, LB_GETCURSEL, 0, 0);

                if ((itemIndex != LB_ERR) && (g_hDevice == INVALID_HANDLE_VALUE))
                {
                    // If we have a selection, enable the open button
                    // If we closed the device, disable the close button
                    EnableWindow(GetDlgItem(hDlg, IDC_OPEN_BUTTON), TRUE);
                }

                return 0;
            }
            break;

        case IDC_OP_TYPE_COMBO:
            switch (HIWORD(wParam))
            {
            case CBN_SELCHANGE:

                hWnd = (HWND)lParam;

                // Get the current selection
                itemIndex = (DWORD)SendMessage(hWnd, CB_GETCURSEL, 0, 0);

                // Get the selection text
                SendMessage(hWnd, CB_GETLBTEXT, (WPARAM)itemIndex, (LPARAM)str);

                if (!_tcscmp(_T("ReadFile"), str))
                {
                    bInBufferEnable = FALSE;
                }

                if (!_tcscmp(_T("WriteFile"), str))
                {
                    bOutBufferEnable = FALSE;
                }

                // Enable the execute button
                EnableWindow(GetDlgItem(hDlg, IDC_EXECUTE_BUTTON), TRUE);

                // Enable/disable the input data pattern edit box
                EnableWindow(GetDlgItem(hDlg, IDC_IN_DATA_STATIC), bInBufferEnable);
                EnableWindow(GetDlgItem(hDlg, IDC_IN_DATA_EDIT), bInBufferEnable);

                // Enable/disable the input buffer size edit box
                EnableWindow(GetDlgItem(hDlg, IDC_IN_SIZE_STATIC), bInBufferEnable);
                EnableWindow(GetDlgItem(hDlg, IDC_IN_SIZE_EDIT), bInBufferEnable);

                // Enable/disable the output data pattern edit box
                EnableWindow(GetDlgItem(hDlg, IDC_OUT_DATA_STATIC), bOutBufferEnable);
                EnableWindow(GetDlgItem(hDlg, IDC_OUT_DATA_EDIT), bOutBufferEnable);

                // Enable/disable the output buffer size edit box
                EnableWindow(GetDlgItem(hDlg, IDC_OUT_SIZE_STATIC), bOutBufferEnable);
                EnableWindow(GetDlgItem(hDlg, IDC_OUT_SIZE_EDIT), bOutBufferEnable);

                return 0;
            }
            break;

        case IDC_EXECUTE_BUTTON:

            testdrvExecuteIo(hDlg);

            // Enable the cancel I/O button
            EnableWindow(GetDlgItem(hDlg, IDC_CANCEL_IO_BUTTON), TRUE);
            break;

        case IDC_CANCEL_IO_BUTTON:

            CancelIo(g_hDevice);

            // Disable the cancel I/O button
            EnableWindow(GetDlgItem(hDlg, IDC_CANCEL_IO_BUTTON), FALSE);
            break;

        default:
            break;
        }
        break;

    case WM_CLOSE:

        if (g_hDevice != INVALID_HANDLE_VALUE)
        {
            CloseHandle(g_hDevice);
            g_hDevice = INVALID_HANDLE_VALUE;
        }

        // Terminate our I/O completion thread
        SetEvent(g_hIoCompletionThreadTerminationEvent);
        WaitForSingleObject(g_hIoCompletionThread, INFINITE);

        UnregisterDeviceNotification(g_hInterfaceNotification);
        EndDialog(hDlg, 0);
        PostQuitMessage(0);

        // Clean up window-specific data objects.
        return 0;

    // Process other messages.

    default:
        return 0;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//  WinMain
//      Application entry point
//
//  Arguments:
//      IN  hInstance
//              Handle to current instance
//
//      IN  hPrevInstance
//              unused
//
//      IN  lpCmdLine
//              command line
//
//      IN  nCmdShow
//              unused
//
//  Return Value:
//      status.
//
int APIENTRY WinMain(
    HINSTANCE   hInstance,
    HINSTANCE   hPrevInstance,
    LPSTR       lpCmdLine,
    int         nCmdShow
                )
{
    // do init stuff
    InitCommonControls();

    // Initialize the global io list
    g_IoList.Next = &g_IoList;
    g_IoList.Previous = &g_IoList;

    // Initialize the global list lock
    InitializeCriticalSection(&g_IoListLock);

    // Initialize the global I/O completion thread termination event.
    // This is a manual reset event
    g_hIoCompletionThreadTerminationEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    UINT nIoCompletionThreadID;

    // Start a new thread to handle I/O completion
    g_hIoCompletionThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        testdrvIoCompletionThread,
        g_hIoCompletionThreadTerminationEvent,
        0,
        &nIoCompletionThreadID
        );

    // start dialog box
    int retVal = (int)DialogBox(hInstance, _T("TESTDRVAPP"), NULL, (DLGPROC)testdrvMainDlgProc);

    // Free allocated resources
    DeleteCriticalSection(&g_IoListLock);
    CloseHandle(g_hIoCompletionThreadTerminationEvent);

    CloseHandle(g_hIoCompletionThread);

    return retVal;
}
