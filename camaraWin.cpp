#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <mferror.h>
#include <stdio.h>
#include <vector>
#include <initguid.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#ifndef GUID_NULL
DEFINE_GUID(GUID_NULL, 0L, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
#endif


IMFSourceReader* g_pReader = nullptr;
HWND g_hwnd = nullptr;
int g_frameWidth = 640;
int g_frameHeight = 480;
std::vector<BYTE> g_frameBuffer;
std::vector<BYTE> g_rgbBuffer; // Buffer temporal para conversión
CRITICAL_SECTION g_criticalSection;
bool g_capturing = false;
GUID g_videoFormat = {0};
UINT32 g_stride = 0;




LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI CaptureThread(LPVOID lpParam);
bool InitializeCamera();
void CleanupCamera();
void DrawFrame(HDC hdc);
void ConvertYUY2toRGB32(const BYTE* pYUY2, BYTE* pRGB32, int width, int height);
inline BYTE Clamp(int value);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    InitializeCriticalSection(&g_criticalSection);
    
    // Crear clase de ventana
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CameraWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);
    
    // Crear ventana
    g_hwnd = CreateWindow(
        L"CameraWindow",
        L"Captura de Cámara - Windows Nativo (Corregido)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_frameWidth + 16, g_frameHeight + 39,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (!g_hwnd) {
        MessageBox(nullptr, L"Error al crear ventana", L"Error", MB_OK);
        return -1;
    }
    
    // Inicializar cámara
    if (!InitializeCamera()) {
        MessageBox(nullptr, L"Error al inicializar cámara", L"Error", MB_OK);
        return -1;
    }
    
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    // Crear thread de captura
    g_capturing = true;
    HANDLE hThread = CreateThread(nullptr, 0, CaptureThread, nullptr, 0, nullptr);
    
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    

    g_capturing = false;
    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    
    CleanupCamera();
    DeleteCriticalSection(&g_criticalSection);
    MFShutdown();
    CoUninitialize();
    
    return 0;
}

bool InitializeCamera() {
    HRESULT hr;

    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return false;
    
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
                              MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        pAttributes->Release();
        return false;
    }
    
    IMFActivate** ppDevices = nullptr;
    UINT32 count;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->Release();
    
    if (FAILED(hr) || count == 0) {
        MessageBox(nullptr, L"No se encontraron cámaras", L"Error", MB_OK);
        return false;
    }
    
    // Usar la primera cámara disponible
    IMFMediaSource* pSource = nullptr;
    hr = ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
    
    // Liberar dispositivos
    for (UINT32 i = 0; i < count; i++) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    
    if (FAILED(hr)) return false;
    
    // Crear source reader
    IMFAttributes* pReaderAttributes = nullptr;
    MFCreateAttributes(&pReaderAttributes, 1);
    pReaderAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    
    hr = MFCreateSourceReaderFromMediaSource(pSource, pReaderAttributes, &g_pReader);
    pSource->Release();
    pReaderAttributes->Release();
    
    if (FAILED(hr)) return false;
    
    // Verificar formato nativo de la cámara
    IMFMediaType* pNativeType = nullptr;
    hr = g_pReader->GetNativeMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,  // Primer tipo de media disponible
        &pNativeType
    );
    
    if (SUCCEEDED(hr)) {
        GUID subtype = {0};
        pNativeType->GetGUID(MF_MT_SUBTYPE, &subtype);
        
        // Debug: mostrar formato nativo de la camara
        wchar_t msg[256];
        if (subtype == MFVideoFormat_NV12) {
            wsprintf(msg, L"Formato nativo detectado: NV12");
        } else if (subtype == MFVideoFormat_YUY2) {
            wsprintf(msg, L"Formato nativo detectado: YUY2");
        } else if (subtype == MFVideoFormat_RGB32) {
            wsprintf(msg, L"Formato nativo detectado: RGB32");
        } else if (subtype == MFVideoFormat_RGB24) {
            wsprintf(msg, L"Formato nativo detectado: RGB24");
        } else {
            wsprintf(msg, L"Formato nativo detectado: Desconocido");
        }
        MessageBox(nullptr, msg, L"Info", MB_OK);
        
        pNativeType->Release();
    }
    
    // Intentar configurar RGB32 primero
    IMFMediaType* pType = nullptr;
    hr = MFCreateMediaType(&pType);
    if (SUCCEEDED(hr)) {
        hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, g_frameWidth, g_frameHeight);
        
        hr = g_pReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            nullptr,
            pType
        );
        
        // Si RGB32 no funciona, intentar con YUY2
        if (FAILED(hr)) {
            pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
            hr = g_pReader->SetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr,
                pType
            );
        }
        
        pType->Release();
    }
    
    // Obtener información del formato actual configurado
    IMFMediaType* pCurrentType = nullptr;
    hr = g_pReader->GetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        &pCurrentType
    );
    
    if (SUCCEEDED(hr)) {
        UINT32 width, height;
        MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &width, &height);
        g_frameWidth = width;
        g_frameHeight = height;
        pCurrentType->GetGUID(MF_MT_SUBTYPE, &g_videoFormat);
        UINT32 stride = 0;
        if (SUCCEEDED(pCurrentType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
            g_stride = stride;
        } else {
            if (g_videoFormat == MFVideoFormat_YUY2) {
                g_stride = g_frameWidth * 2; // YUY2 = 2 bytes por pixel
            } else {
                g_stride = g_frameWidth * 4; // RGB32 = 4 bytes por pixel
            }
        }
        
        // Debug: mostrar formato configurado
        wchar_t msg[256];
        if (g_videoFormat == MFVideoFormat_YUY2) {
            wsprintf(msg, L"Formato configurado: YUY2 (%dx%d)", width, height);
        } else if (g_videoFormat == MFVideoFormat_RGB32) {
            wsprintf(msg, L"Formato configurado: RGB32 (%dx%d)", width, height);
        } else {
            wsprintf(msg, L"Formato configurado: Otro (%dx%d)", width, height);
        }
        MessageBox(nullptr, msg, L"Info", MB_OK);
        
        pCurrentType->Release();
    }
    
    // Ajustar tamaño de buffers según el formato
    if (g_videoFormat == MFVideoFormat_YUY2) {
        g_frameBuffer.resize(g_frameWidth * g_frameHeight * 2); // YUY2 = 2 bytes por pixel
        g_rgbBuffer.resize(g_frameWidth * g_frameHeight * 4);   // RGB32 para conversión
    } else {
        g_frameBuffer.resize(g_frameWidth * g_frameHeight * 4); // RGB32 = 4 bytes por pixel
    }
    
    return true;
}

DWORD WINAPI CaptureThread(LPVOID lpParam) {
    while (g_capturing) {
        HRESULT hr;
        DWORD streamIndex, flags;
        LONGLONG llTimeStamp;
        IMFSample* pSample = nullptr;
        hr = g_pReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &streamIndex,
            &flags,
            &llTimeStamp,
            &pSample
        );
        
        if (SUCCEEDED(hr) && pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            
            if (SUCCEEDED(hr)) {
                BYTE* pData = nullptr;
                DWORD cbCurrentLength = 0;
                
                hr = pBuffer->Lock(&pData, nullptr, &cbCurrentLength);
                if (SUCCEEDED(hr)) {
                    EnterCriticalSection(&g_criticalSection);
                    
                    // Manejar según el formato
                    if (g_videoFormat == MFVideoFormat_YUY2) {
                        // Convertir YUY2 a RGB32
                        ConvertYUY2toRGB32(pData, g_rgbBuffer.data(), 
                                         g_frameWidth, g_frameHeight);
                        // El buffer de display será el RGB convertido
                    } else if (g_videoFormat == MFVideoFormat_RGB32) {
                        // Copiar directamente si ya es RGB32
                        if (cbCurrentLength <= g_frameBuffer.size()) {
                            memcpy(g_frameBuffer.data(), pData, cbCurrentLength);
                        }
                    }
                    
                    LeaveCriticalSection(&g_criticalSection);
                    
                    pBuffer->Unlock();
                    InvalidateRect(g_hwnd, nullptr, FALSE);
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
        
        // Pequeña pausa para no saturar el CPU
        Sleep(30); // ~33 FPS
    }
    return 0;
}

void ConvertYUY2toRGB32(const BYTE* pYUY2, BYTE* pRGB32, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += 2) {
            int yuy2Index = (y * width + x) * 2;
            int rgb32Index = (y * width + x) * 4;
            
            BYTE y1 = pYUY2[yuy2Index + 0];
            BYTE u  = pYUY2[yuy2Index + 1];
            BYTE y2 = pYUY2[yuy2Index + 2];
            BYTE v  = pYUY2[yuy2Index + 3];
            
            // Convertir YUV a RGB usando coeficientes ITU-R BT.601
            int c1 = y1 - 16;
            int c2 = y2 - 16;
            int d = u - 128;
            int e = v - 128;
            
            // Primer pixel
            pRGB32[rgb32Index + 0] = Clamp((298 * c1 + 516 * d + 128) >> 8);           // B
            pRGB32[rgb32Index + 1] = Clamp((298 * c1 - 100 * d - 208 * e + 128) >> 8); // G
            pRGB32[rgb32Index + 2] = Clamp((298 * c1 + 409 * e + 128) >> 8);           // R
            pRGB32[rgb32Index + 3] = 255;                                               // A
            
            // Segundo pixel (si existe)
            if (x + 1 < width) {
                pRGB32[rgb32Index + 4] = Clamp((298 * c2 + 516 * d + 128) >> 8);           // B
                pRGB32[rgb32Index + 5] = Clamp((298 * c2 - 100 * d - 208 * e + 128) >> 8); //G
                pRGB32[rgb32Index + 6] = Clamp((298 * c2 + 409 * e + 128) >> 8);           // R
                pRGB32[rgb32Index + 7] = 255;                                               // A
            }
        }
    }
}

inline BYTE Clamp(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (BYTE)value;
}

void DrawFrame(HDC hdc) {
    EnterCriticalSection(&g_criticalSection);
    
    // Crear bitmap para mostrar el frame
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_frameWidth;
    bmi.bmiHeader.biHeight = -g_frameHeight; // Negativo para que este derecho
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // Usar el buffer apropiado segn el formato
    BYTE* displayBuffer = nullptr;
    if (g_videoFormat == MFVideoFormat_YUY2) {
        displayBuffer = g_rgbBuffer.data(); // Buffer convertido a RGB32
    } else {
        displayBuffer = g_frameBuffer.data(); // Buffer RGB32 original
    }
    
    // Dibujar el bitmap en el dispositivo
    SetDIBitsToDevice(
        hdc,
        0, 0,
        g_frameWidth, g_frameHeight,
        0, 0,
        0, g_frameHeight,
        displayBuffer,
        &bmi,
        DIB_RGB_COLORS
    );
    
    LeaveCriticalSection(&g_criticalSection);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawFrame(hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CleanupCamera() {
    if (g_pReader) {
        g_pReader->Release();
        g_pReader = nullptr;
    }
}