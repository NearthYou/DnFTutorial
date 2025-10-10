#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "windowscodecs")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdio>

// 텍스처 사각형 정점
struct FVertexTex
{
    float x, y, z;    // Position (NDC)
    float u, v;       // UV
};

class URenderer
{
public:
    // Direct3D 11 장치(Device)와 장치 컨텍스트(Device Context) 및 스왑 체인(Swap Chain)을 관리하기 위한 포인터들
    ID3D11Device* Device = nullptr; // GPU와 통신하기 위한 Direct3D 장치
    ID3D11DeviceContext* DeviceContext = nullptr; // GPU 명령 실행을 담당하는 컨텍스트
    IDXGISwapChain* SwapChain = nullptr; // 프레임 버퍼를 교체하는 데 사용되는 스왑 체인

    // 렌더링에 필요한 리소스 및 상태를 관리하기 위한 변수들
    ID3D11Texture2D* FrameBuffer = nullptr; // 화면 출력용 텍스처
    ID3D11RenderTargetView* FrameBufferRTV = nullptr; // 텍스처를 렌더 타겟으로 사용하는 뷰
    ID3D11RasterizerState* RasterizerState = nullptr; // 래스터라이저 상태(컬링, 채우기 모드 등 정의)
    ID3D11Buffer* ConstantBuffer = nullptr; // 쉐이더에 데이터를 전달하기 위한 상수 버퍼

    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // 화면을 초기화(clear)할 때 사용할 색상 (RGBA)
    D3D11_VIEWPORT ViewportInfo; // 렌더링 영역을 정의하는 뷰포트 정보

    ID3D11VertexShader* SimpleVertexShader;
    ID3D11PixelShader* SimplePixelShader;
    ID3D11InputLayout* SimpleInputLayout;
    unsigned int Stride;

    // 텍스처 렌더링에 필요한 리소스
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    ID3D11SamplerState* TextureSampler = nullptr;

public:
    // 렌더러 초기화 함수
    void Create(HWND hWindow)
    {
        // Direct3D 장치 및 스왑 체인 생성
        CreateDeviceAndSwapChain(hWindow);

        // 프레임 버퍼 생성
        CreateFrameBuffer();

        // 래스터라이저 상태 생성
        CreateRasterizerState();

        // 깊이 스텐실 버퍼 및 블렌드 상태는 이 코드에서는 다루지 않음
    }

    // Direct3D 장치 및 스왑 체인을 생성하는 함수
    void CreateDeviceAndSwapChain(HWND hWindow)
    {
        // 지원하는 Direct3D 기능 레벨을 정의
        D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

        // 스왑 체인 설정 구조체 초기화
        DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
        swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
        swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
        swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
        swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
        swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
        swapchaindesc.BufferCount = 2; // 더블 버퍼링
        swapchaindesc.OutputWindow = hWindow; // 렌더링할 창 핸들
        swapchaindesc.Windowed = TRUE; // 창 모드
        swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

        // Direct3D 장치와 스왑 체인을 생성
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
            featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
            &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

        // 생성된 스왑 체인의 정보 가져오기
        SwapChain->GetDesc(&swapchaindesc);

        // 뷰포트 정보 설정
        ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
    }

    // Direct3D 장치 및 스왑 체인을 해제하는 함수
    void ReleaseDeviceAndSwapChain()
    {
        if (DeviceContext)
        {
            DeviceContext->Flush(); // 남아있는 GPU 명령 실행
        }

        if (SwapChain)
        {
            SwapChain->Release();
            SwapChain = nullptr;
        }

        if (Device)
        {
            Device->Release();
            Device = nullptr;
        }

        if (DeviceContext)
        {
            DeviceContext->Release();
            DeviceContext = nullptr;
        }
    }

    // 프레임 버퍼를 생성하는 함수
    void CreateFrameBuffer()
    {
        // 스왑 체인으로부터 백 버퍼 텍스처 가져오기
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

        // 렌더 타겟 뷰 생성
        D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
        framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // 색상 포맷
        framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D 텍스처

        Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
    }

    // 프레임 버퍼를 해제하는 함수
    void ReleaseFrameBuffer()
    {
        if (FrameBuffer)
        {
            FrameBuffer->Release();
            FrameBuffer = nullptr;
        }

        if (FrameBufferRTV)
        {
            FrameBufferRTV->Release();
            FrameBufferRTV = nullptr;
        }
    }

    // 래스터라이저 상태를 생성하는 함수
    void CreateRasterizerState()
    {
        D3D11_RASTERIZER_DESC rasterizerdesc = {};
        rasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
        rasterizerdesc.CullMode = D3D11_CULL_NONE; // 풀스크린 사각형은 컬링 비활성화가 안전

        Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
    }

    // 래스터라이저 상태를 해제하는 함수
    void ReleaseRasterizerState()
    {
        if (RasterizerState)
        {
            RasterizerState->Release();
            RasterizerState = nullptr;
        }
    }

    // 렌더러에 사용된 모든 리소스를 해제하는 함수
    void Release()
    {
        RasterizerState->Release();

        // 렌더 타겟을 초기화
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

        ReleaseFrameBuffer();
        ReleaseDeviceAndSwapChain();
    }

    // 스왑 체인의 백 버퍼와 프론트 버퍼를 교체하여 화면에 출력
    void SwapBuffer()
    {
        SwapChain->Present(1, 0); // 1: VSync 활성화
    }

	void CreateShader()
	{
		ID3DBlob* vertexshaderCSO;
		ID3DBlob* pixelshaderCSO;

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

		Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

		D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

		Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

		Stride = sizeof(FVertexTex);

		vertexshaderCSO->Release();
		pixelshaderCSO->Release();
	}
	
	void ReleaseShader()
	{
		if (SimpleInputLayout)
		{
			SimpleInputLayout->Release();
			SimpleInputLayout = nullptr;
		}

		if (SimplePixelShader)
		{
			SimplePixelShader->Release();
			SimplePixelShader = nullptr;
		}

		if (SimpleVertexShader)
		{
			SimpleVertexShader->Release();
			SimpleVertexShader = nullptr;
		}
	}

    void CreateTextureSampler()
    {
        D3D11_SAMPLER_DESC samp{};
        samp.Filter = D3D11_FILTER_ANISOTROPIC;
        samp.MaxAnisotropy = 8;
        samp.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samp.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samp.MaxLOD = D3D11_FLOAT32_MAX;
        Device->CreateSamplerState(&samp, &TextureSampler);
    }

    void ReleaseTexture()
    {
        if (TextureSampler)
        {
            TextureSampler->Release();
            TextureSampler = nullptr;
        }
        if (TextureSRV)
        {
            TextureSRV->Release();
            TextureSRV = nullptr;
        }
    }

    void Prepare()
    {
        DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

        DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        DeviceContext->RSSetViewports(1, &ViewportInfo);
        DeviceContext->RSSetState(RasterizerState);

        DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
        DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }

    void PrepareShader()
    {
        DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
        DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
        DeviceContext->IASetInputLayout(SimpleInputLayout);
        DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
        DeviceContext->PSSetSamplers(0, 1, &TextureSampler);
        DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    }

    void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
    {
        UINT offset = 0;
        DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);

        DeviceContext->Draw(numVertices, 0);
    }
};
// 간단한 WIC 로더: 파일을 32bpp BGRA로 변환해 텍스처/ SRV 생성
HRESULT CreateTextureFromFileWIC(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* path, ID3D11ShaderResourceView** outSRV)
{
    *outSRV = nullptr;

    IWICImagingFactory* wicFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) return hr;

    IWICBitmapDecoder* decoder = nullptr;
    hr = wicFactory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { wicFactory->Release(); return hr; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { decoder->Release(); wicFactory->Release(); return hr; }

    IWICFormatConverter* converter = nullptr;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { frame->Release(); decoder->Release(); wicFactory->Release(); return hr; }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return hr; }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);

    const UINT stride = width * 4;
    const UINT imageSize = stride * height;
    BYTE* pixels = new BYTE[imageSize];
    hr = converter->CopyPixels(nullptr, stride, imageSize, pixels);
    if (FAILED(hr)) { delete[] pixels; converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return hr; }

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 0; // 전체 밉체인 자동 생성
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* texture = nullptr;
    // 밉 자동 생성을 위해 초기 데이터 없이 생성 후, 레벨0을 UpdateSubresource로 채움
    hr = device->CreateTexture2D(&texDesc, nullptr, &texture);
    if (SUCCEEDED(hr))
    {
        // 레벨0 업로드
        context->UpdateSubresource(texture, 0, nullptr, pixels, stride, 0);

        // SRV 생성 (전체 밉 사용)
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = -1; // 모든 밉
        hr = device->CreateShaderResourceView(texture, &srvDesc, outSRV);
        if (SUCCEEDED(hr))
        {
            // 밉맵 생성
            context->GenerateMips(*outSRV);
        }

        texture->Release();
    }

    delete[] pixels;
    converter->Release();
    frame->Release();
    decoder->Release();
    wicFactory->Release();
    return hr;
}

// 간단한 DDS 로더: DXT1 / DXT5 (BC1/BC3, sRGB)만 지원, 2D 텍스처/미입력 어레이
HRESULT CreateTextureFromFileDDS(ID3D11Device* device, const wchar_t* path, ID3D11ShaderResourceView** outSRV)
{
    *outSRV = nullptr;

    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") != 0 || !f) return E_FAIL;

    auto readU32 = [&](uint32_t& v)->bool { return fread(&v, 1, 4, f) == 4; };

    uint32_t magic = 0;
    if (!readU32(magic)) { fclose(f); return E_FAIL; }
    if (magic != 0x20534444) { fclose(f); return E_FAIL; } // 'DDS '

    struct DDS_PIXELFORMAT { uint32_t size, flags, fourCC, rgbBitCount, rMask, gMask, bMask, aMask; };
    struct DDS_HEADER {
        uint32_t size, flags, height, width, pitchOrLinearSize, depth, mipMapCount;
        uint32_t reserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t caps, caps2, caps3, caps4, reserved2;
    } header{};

    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) { fclose(f); return E_FAIL; }

    const uint32_t FOURCC_DXT1 = 0x31545844; // 'DXT1'
    const uint32_t FOURCC_DXT5 = 0x35545844; // 'DXT5'

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t blockSize = 0;
    if (header.ddspf.fourCC == FOURCC_DXT1) { format = DXGI_FORMAT_BC1_UNORM_SRGB; blockSize = 8; }
    else if (header.ddspf.fourCC == FOURCC_DXT5) { format = DXGI_FORMAT_BC3_UNORM_SRGB; blockSize = 16; }
    else { fclose(f); return E_FAIL; }

    const uint32_t width = header.width;
    const uint32_t height = header.height;
    const uint32_t mipLevels = header.mipMapCount ? header.mipMapCount : 1;

    // 남은 바이너리 전체 읽기
    fseek(f, 0, SEEK_END);
    long fileEnd = ftell(f);
    long dataOffset = 4 + static_cast<long>(sizeof(DDS_HEADER));
    long dataSizeAll = fileEnd - dataOffset;
    if (dataSizeAll <= 0) { fclose(f); return E_FAIL; }
    std::vector<uint8_t> buffer;
    buffer.resize(static_cast<size_t>(dataSizeAll));
    fseek(f, dataOffset, SEEK_SET);
    size_t readBytes = fread(buffer.data(), 1, static_cast<size_t>(dataSizeAll), f);
    fclose(f);
    if (readBytes != static_cast<size_t>(dataSizeAll)) return E_FAIL;

    // 서브리소스 구성
    std::vector<D3D11_SUBRESOURCE_DATA> subs;
    subs.reserve(mipLevels);
    size_t offset = 0;
    uint32_t w = width, h = height;
    for (uint32_t i = 0; i < mipLevels; ++i)
    {
        uint32_t bw = (w + 3) / 4;
        uint32_t bh = (h + 3) / 4;
        size_t levelSize = static_cast<size_t>(bw) * static_cast<size_t>(bh) * blockSize;
        if (offset + levelSize > buffer.size()) return E_FAIL;

        D3D11_SUBRESOURCE_DATA srd{};
        srd.pSysMem = buffer.data() + offset;
        srd.SysMemPitch = bw * blockSize;
        srd.SysMemSlicePitch = 0;
        subs.push_back(srd);

        offset += levelSize;
        w = (w > 1) ? (w >> 1) : 1;
        h = (h > 1) ? (h >> 1) : 1;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipLevels;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, subs.data(), &tex);
    if (FAILED(hr)) return hr;
    hr = device->CreateShaderResourceView(tex, nullptr, outSRV);
    tex->Release();
    return hr;
}

// 각종 메시지를 처리할 함수
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		// Signal that the app should quit
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	// 윈도우 클래스 이름
	WCHAR WindowClass[] = L"WindowClass";

	// 윈도우 타이틀바에 표시될 이름
	WCHAR Title[] = L"My Window";

	// 각종 메시지를 처리할 함수인 WndProc의 함수 포인터를 WindowClass 구조체에 넣는다.
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	// 윈도우 클래스 등록
	RegisterClassW(&wndclass);

	// 1024 x 1024 크기에 윈도우 생성
	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);

	bool bIsExit = false;

    ///////////////////////////////////////////////
	// 각종 생성하는 코드를 여기에 추가.

    // COM 초기화 (WIC 사용)
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Renderer Class를 생성.
    URenderer renderer;

    // D3D11 생성하는 함수를 호출.
    renderer.Create(hWnd);
    renderer.CreateShader();

    // 텍스처 로드 및 샘플러 생성
    renderer.CreateTextureSampler();
    // DDS(BC1/BC3) 로드
    if (FAILED(CreateTextureFromFileDDS(renderer.Device, L"Sprites/Character.dds", &renderer.TextureSRV)))
    {
        // 폴백: 기존 WIC 로더
        CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Character.jpg", &renderer.TextureSRV);
    }

    // 상수 버퍼 생성 (Offset.xy, Scale.xy)
    {
        D3D11_BUFFER_DESC cbd{};
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.ByteWidth = sizeof(float) * 4;
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        renderer.Device->CreateBuffer(&cbd, nullptr, &renderer.ConstantBuffer);
    }

    // 풀스크린 사각형 정점 버퍼 생성 (TRIANGLELIST 6개 정점)
    FVertexTex quad[] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
    };

    UINT numVertices = sizeof(quad) / sizeof(FVertexTex);

    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = sizeof(quad);
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = { quad };

    ID3D11Buffer* vertexBuffer;

    renderer.Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    // 이동/점프 상태 변수
    float posX = 0.0f;   // NDC 기준 위치
    float posY = 0.0f;
    float scaleX = 0.25f; // 화면에서 보이는 스프라이트 크기
    float scaleY = 0.25f;
    float velocityY = 0.0f;
    const float gravity = -2.5f;   // 초당 중력 가속
    const float jumpSpeed = 1.6f;  // 점프 초기 속도
    const float moveSpeed = 1.6f;  // 좌우 이동 속도

    // 시간 측정용
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    LARGE_INTEGER prev; QueryPerformanceCounter(&prev);

    // Main Loop (Quit Message가 들어오기 전까지 아래 Loop를 무한히 실행하게 됨)
	while (bIsExit == false)
	{
		MSG msg;

		// 처리할 메시지가 더 이상 없을때 까지 수행
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			// 키 입력 메시지를 번역
			TranslateMessage(&msg);

			// 메시지를 적절한 윈도우 프로시저에 전달, 메시지가 위에서 등록한 WndProc 으로 전달됨
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}
        ///////////////////////////////////////////////
        // // 매번 실행되는 코드를 여기에 추가.
  
        // deltaTime 계산
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        float dt = float(now.QuadPart - prev.QuadPart) / float(freq.QuadPart);
        prev = now;

        // 입력 처리
        SHORT left  = GetAsyncKeyState(VK_LEFT);
        SHORT right = GetAsyncKeyState(VK_RIGHT);
        SHORT up    = GetAsyncKeyState(VK_UP);
        SHORT down  = GetAsyncKeyState(VK_DOWN);
        SHORT space = GetAsyncKeyState(VK_SPACE);

        if (left & 0x8000)  posX -= moveSpeed * dt;
        if (right & 0x8000) posX += moveSpeed * dt;
        if (down & 0x8000)  posY -= moveSpeed * dt;
        if (up & 0x8000)    posY += moveSpeed * dt;

        // 스페이스 점프(바닥에 있을 때만)
        bool onGround = (posY - scaleY) <= -1.0f;
        if (onGround)
        {
            posY = -1.0f + scaleY; // 바닥에 스냅
            velocityY = 0.0f;
            if (space & 0x8000) velocityY = jumpSpeed;
        }
        else
        {
            // 중력 적용
            velocityY += gravity * dt;
        }

        // 수직 이동에 속도 반영
        posY += velocityY * dt;

        // 화면 경계(NDC) 클램프: 스케일을 고려해 스프라이트가 화면 밖으로 안 나가도록
        float halfW = scaleX;
        float halfH = scaleY;
        if (posX - halfW < -1.0f) posX = -1.0f + halfW;
        if (posX + halfW >  1.0f) posX =  1.0f - halfW;
        if (posY - halfH < -1.0f) posY = -1.0f + halfH;
        if (posY + halfH >  1.0f) posY =  1.0f - halfH;

        // 상수 버퍼 업데이트(Offset.xy, Scale.xy)
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                float* cb = (float*)mapped.pData;
                cb[0] = posX; cb[1] = posY; cb[2] = scaleX; cb[3] = scaleY;
                renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
            }
        }

        // 준비 작업
        renderer.Prepare();
        renderer.PrepareShader();

        // 생성한 버텍스 버퍼를 넘겨 실질적인 렌더링 요청
        renderer.RenderPrimitive(vertexBuffer, numVertices);
        // 현재 화면에 보여지는 버퍼와 그리기 작업을 위한 버퍼를 서로 교환
        renderer.SwapBuffer();

	}

    ///////////////////////////////////////////////
	// 소멸하는 코드를 여기에 추가.

    // D3D11 소멸 시키는 함수를 호출.

    vertexBuffer->Release();
    renderer.ReleaseTexture();
    renderer.ReleaseShader();
    renderer.Release();
    if (renderer.ConstantBuffer) { renderer.ConstantBuffer->Release(); renderer.ConstantBuffer = nullptr; }

    CoUninitialize();

	return 0;
}
