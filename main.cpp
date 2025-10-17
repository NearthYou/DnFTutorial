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
#include <algorithm>

// 텍스처 사각형 정점
struct FVertexTex
{
    float x, y, z;    // Position (NDC)
    float u, v;       // UV
};

// URenderer 클래스 (게임 오브젝트들보다 앞에 정의)
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
    ID3D11ShaderResourceView* TextureSRV = nullptr;  // 플레이어 텍스처
    ID3D11ShaderResourceView* MonsterTextureSRV = nullptr;  // 몬스터 텍스처
    ID3D11ShaderResourceView* SkillTextureSRV = nullptr;  // 스킬 텍스처
    ID3D11SamplerState* TextureSampler = nullptr;
    ID3D11BlendState* BlendState = nullptr;  // 투명도 처리를 위한 블렌드 상태
    
    // 공통 버텍스 버퍼
    ID3D11Buffer* VertexBuffer = nullptr;

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

    void CreateBlendState()
    {
        D3D11_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        
        Device->CreateBlendState(&blendDesc, &BlendState);
    }

    void ReleaseTexture()
    {
        if (BlendState)
        {
            BlendState->Release();
            BlendState = nullptr;
        }
        if (TextureSampler)
        {
            TextureSampler->Release();
            TextureSampler = nullptr;
        }
        if (MonsterTextureSRV)
        {
            MonsterTextureSRV->Release();
            MonsterTextureSRV = nullptr;
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
        
        // 투명도 처리를 위한 블렌드 상태 설정
        if (BlendState)
        {
            DeviceContext->OMSetBlendState(BlendState, nullptr, 0xffffffff);
        }
        else
        {
            DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        }
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

// 카메라 시스템
struct FCamera
{
    float PositionX;  // 카메라 월드 X 위치
    float PositionY;  // 카메라 월드 Y 위치
    float ViewWidth;  // 뷰포트 너비
    float ViewHeight; // 뷰포트 높이
    
    FCamera() : PositionX(0.0f), PositionY(0.0f), ViewWidth(4.0f), ViewHeight(3.0f) {}
    
    // 월드 좌표를 화면 좌표로 변환
    void WorldToScreen(float worldX, float worldY, float& screenX, float& screenY)
    {
        screenX = (worldX - PositionX) / (ViewWidth * 0.5f);
        screenY = (worldY - PositionY) / (ViewHeight * 0.5f);
    }
    
    // 화면 좌표를 월드 좌표로 변환
    void ScreenToWorld(float screenX, float screenY, float& worldX, float& worldY)
    {
        worldX = screenX * (ViewWidth * 0.5f) + PositionX;
        worldY = screenY * (ViewHeight * 0.5f) + PositionY;
    }
    
    // 플레이어를 따라가도록 카메라 위치 업데이트
    void FollowPlayer(float playerX, float playerY)
    {
        // 부드러운 카메라 이동 (선형 보간)
        float targetX = playerX;
        float targetY = playerY;
        
        PositionX += (targetX - PositionX) * 0.1f;
        PositionY += (targetY - PositionY) * 0.1f;
    }
};

// 콜라이더 구조체
struct FCollider
{
    float centerX, centerY;  // 중심점
    float halfWidth, halfHeight;  // 반폭, 반높이
    
    FCollider(float x = 0.0f, float y = 0.0f, float w = 0.0f, float h = 0.0f)
        : centerX(x), centerY(y), halfWidth(w), halfHeight(h) {}
    
    // AABB 충돌 감지
    bool CheckCollision(const FCollider& other) const
    {
        return (abs(centerX - other.centerX) < (halfWidth + other.halfWidth)) &&
               (abs(centerY - other.centerY) < (halfHeight + other.halfHeight));
    }
    
    // 위치 업데이트
    void UpdatePosition(float x, float y)
    {
        centerX = x;
        centerY = y;
    }
};

// 게임 오브젝트 타입
enum class EGameObjectType
{
    Player,
    Monster,
    Attack,
};

// 기본 게임 오브젝트 클래스
class UGameObject
{
public:
    EGameObjectType Type;
    float PositionX, PositionY;  // 월드 좌표
    float ScaleX, ScaleY;
    FCollider Collider;
    bool bIsActive;
    bool bFacingRight;  // 오른쪽을 보고 있는지 (던파 스타일)
    
    UGameObject(EGameObjectType type, float x = 0.0f, float y = 0.0f, float sx = 0.5f, float sy = 0.5f)
        : Type(type), PositionX(x), PositionY(y), ScaleX(sx), ScaleY(sy), bIsActive(true), bFacingRight(true)
    {
        Collider = FCollider(x, y, sx, sy);
    }
    
    virtual void Update(float deltaTime) {}
    virtual void Render(URenderer& renderer, FCamera& camera);
    
    void UpdateCollider()
    {
        Collider.UpdatePosition(PositionX, PositionY);
    }
    
    // 방향 전환
    void SetFacingRight(bool facingRight)
    {
        bFacingRight = facingRight;
    }
};

// 허수아비 몬스터 클래스
class UMonster : public UGameObject
{
public:
    float Health;
    float MaxHealth;
    bool bIsAlive;
    float MoveSpeed;
    float Direction;  // 이동 방향 (-1 또는 1)
    
    UMonster(float x = 0.0f, float y = 0.0f) 
        : UGameObject(EGameObjectType::Monster, x, y, 0.4f, 0.6f)
        , Health(100.0f), MaxHealth(100.0f), bIsAlive(true), MoveSpeed(1.0f), Direction(-1.0f)
    {
        UpdateCollider();
    }
    
    void TakeDamage(float damage)
    {
        Health -= damage;
        if (Health <= 0.0f)
        {
            Health = 0.0f;
            bIsAlive = false;
            bIsActive = false;
        }
    }
    
    void Update(float deltaTime) override
    {
        if (!bIsAlive) return;
        
        // 벨트 위에서 좌우로 이동
        PositionX += Direction * MoveSpeed * deltaTime;
        
        // 화면 경계에서 방향 전환
        if (PositionX < -2.0f || PositionX > 2.0f)
        {
            Direction *= -1.0f;
            bFacingRight = Direction > 0.0f;
        }
        
        UpdateCollider();
    }
    
    void Render(URenderer& renderer, FCamera& camera) override;
};

// 공격 오브젝트 클래스
class UAttackObject : public UGameObject
{
public:
    float Damage;
    float LifeTime;
    float CurrentLifeTime;
    UGameObject* Owner;
    float AttackRange;
    float AttackDirection;  // 공격 방향 (-1 또는 1)
    
    UAttackObject(float x, float y, float damage, UGameObject* owner, float direction)
        : UGameObject(EGameObjectType::Attack, x, y, 0.3f, 0.3f)
        , Damage(damage), LifeTime(0.2f), CurrentLifeTime(0.0f), Owner(owner)
        , AttackRange(0.8f), AttackDirection(direction)
    {
        // 공격 방향에 따라 위치 조정
        PositionX += AttackDirection * AttackRange * 0.5f;
        UpdateCollider();
    }
    
    void Update(float deltaTime) override
    {
        CurrentLifeTime += deltaTime;
        
        // 공격 애니메이션 효과 (크기 변화)
        float progress = CurrentLifeTime / LifeTime;
        float scaleMultiplier = 1.0f + sin(progress * 3.14159f * 2.0f) * 0.3f;
        ScaleX = 0.15f * scaleMultiplier;
        ScaleY = 0.15f * scaleMultiplier;
        
        // 공격이 진행되면서 앞으로 이동
        PositionX += AttackDirection * 1.5f * deltaTime;
        
        if (CurrentLifeTime >= LifeTime)
        {
            bIsActive = false;
        }
        UpdateCollider();
    }
    
    void Render(URenderer& renderer, FCamera& camera) override;
};

// 플레이어 클래스
class UPlayer : public UGameObject
{
public:
    float VelocityY;
    float AttackCooldown;
    float CurrentAttackCooldown;
    float MoveSpeed;
    bool bOnGround;
    float GroundY;  // 바닥 Y 좌표
    
    UPlayer(float x = 0.0f, float y = 0.0f)
        : UGameObject(EGameObjectType::Player, x, y, 0.4f, 0.6f)
        , VelocityY(0.0f), AttackCooldown(0.3f), CurrentAttackCooldown(0.0f)
        , MoveSpeed(3.0f), bOnGround(false), GroundY(0.0f)
    {
        UpdateCollider();
    }
    
    void Update(float deltaTime) override
    {
        if (CurrentAttackCooldown > 0.0f)
        {
            CurrentAttackCooldown -= deltaTime;
        }
        
        // 중력 적용
        if (!bOnGround)
        {
            VelocityY -= 8.0f * deltaTime;  // 중력
            PositionY += VelocityY * deltaTime;
            
            // 바닥에 착지
            if (PositionY <= GroundY)
            {
                PositionY = GroundY;
                VelocityY = 0.0f;
                bOnGround = true;
            }
        }
        
        UpdateCollider();
    }
    
    void MoveLeft(float deltaTime)
    {
        PositionX -= MoveSpeed * deltaTime;
        bFacingRight = false;
    }
    
    void MoveRight(float deltaTime)
    {
        PositionX += MoveSpeed * deltaTime;
        bFacingRight = true;
    }
    
    void Jump()
    {
        if (bOnGround)
        {
            VelocityY = 4.0f;
            bOnGround = false;
        }
    }
    
    bool CanAttack() const
    {
        return CurrentAttackCooldown <= 0.0f;
    }
    
    void StartAttackCooldown()
    {
        CurrentAttackCooldown = AttackCooldown;
    }
    
    float GetAttackDirection() const
    {
        return bFacingRight ? 1.0f : -1.0f;
    }
    
    void Render(URenderer& renderer, FCamera& camera) override;
};


// UGameObject Render 함수 구현
void UGameObject::Render(URenderer& renderer, FCamera& camera)
{
    // 기본 구현은 비어있음
}

void UMonster::Render(URenderer& renderer, FCamera& camera)
{
    if (!bIsActive) return;
    
    // 월드 좌표를 화면 좌표로 변환
    float screenX, screenY;
    camera.WorldToScreen(PositionX, PositionY, screenX, screenY);
    
    // 상수 버퍼 업데이트
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* cb = (float*)mapped.pData;
        cb[0] = screenX; cb[1] = screenY; cb[2] = ScaleX; cb[3] = ScaleY;
        renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
    }
    
    // 몬스터 전용 텍스처 사용
    if (renderer.MonsterTextureSRV)
    {
        renderer.DeviceContext->PSSetShaderResources(0, 1, &renderer.MonsterTextureSRV);
    }
    
    // 렌더링
    renderer.RenderPrimitive(renderer.VertexBuffer, 6);
}

void UAttackObject::Render(URenderer& renderer, FCamera& camera)
{
    if (!bIsActive) return;
    
    // 월드 좌표를 화면 좌표로 변환
    float screenX, screenY;
    camera.WorldToScreen(PositionX, PositionY, screenX, screenY);
    
    // 상수 버퍼 업데이트
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* cb = (float*)mapped.pData;
        cb[0] = screenX; cb[1] = screenY; cb[2] = ScaleX; cb[3] = ScaleY;
        renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
    }
    
    // 스킬 텍스처 사용 (Skill.gif)
    if (renderer.SkillTextureSRV)
    {
        renderer.DeviceContext->PSSetShaderResources(0, 1, &renderer.SkillTextureSRV);
    }
    
    renderer.RenderPrimitive(renderer.VertexBuffer, 6);
}

void UPlayer::Render(URenderer& renderer, FCamera& camera)
{
    // 월드 좌표를 화면 좌표로 변환
    float screenX, screenY;
    camera.WorldToScreen(PositionX, PositionY, screenX, screenY);
    
    // 상수 버퍼 업데이트
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* cb = (float*)mapped.pData;
        cb[0] = screenX; cb[1] = screenY; cb[2] = ScaleX; cb[3] = ScaleY;
        renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
    }
    
    // 플레이어 텍스처 사용
    if (renderer.TextureSRV)
    {
        renderer.DeviceContext->PSSetShaderResources(0, 1, &renderer.TextureSRV);
    }
    
    renderer.RenderPrimitive(renderer.VertexBuffer, 6);
}

// 게임 오브젝트 관리자
class UGameObjectManager
{
public:
    std::vector<UGameObject*> GameObjects;
    UPlayer* Player;
    
    UGameObjectManager() : Player(nullptr) {}
    
    ~UGameObjectManager()
    {
        for (auto obj : GameObjects)
        {
            delete obj;
        }
        GameObjects.clear();
    }
    
    void AddGameObject(UGameObject* obj)
    {
        GameObjects.push_back(obj);
        if (obj->Type == EGameObjectType::Player)
        {
            Player = static_cast<UPlayer*>(obj);
        }
    }
    
    void RemoveGameObject(UGameObject* obj)
    {
        auto it = std::find(GameObjects.begin(), GameObjects.end(), obj);
        if (it != GameObjects.end())
        {
            GameObjects.erase(it);
            delete obj;
        }
    }
    
    void Update(float deltaTime)
    {
        // 비활성 오브젝트 제거
        for (auto it = GameObjects.begin(); it != GameObjects.end();)
        {
            if (!(*it)->bIsActive)
            {
                delete *it;
                it = GameObjects.erase(it);
            }
            else
            {
                (*it)->Update(deltaTime);
                ++it;
            }
        }
        
        // 충돌 처리
        ProcessCollisions();
    }
    
    void Render(URenderer& renderer, FCamera& camera)
    {
        for (auto obj : GameObjects)
        {
            if (obj->bIsActive)
            {
                obj->Render(renderer, camera);
            }
        }
    }
    
    void ProcessCollisions()
    {
        for (size_t i = 0; i < GameObjects.size(); ++i)
        {
            for (size_t j = i + 1; j < GameObjects.size(); ++j)
            {
                UGameObject* obj1 = GameObjects[i];
                UGameObject* obj2 = GameObjects[j];
                
                if (!obj1->bIsActive || !obj2->bIsActive) continue;
                
                // 충돌 감지
                if (obj1->Collider.CheckCollision(obj2->Collider))
                {
                    HandleCollision(obj1, obj2);
                }
            }
        }
    }
    
    void HandleCollision(UGameObject* obj1, UGameObject* obj2)
    {
        // 공격 오브젝트와 몬스터 충돌
        if ((obj1->Type == EGameObjectType::Attack && obj2->Type == EGameObjectType::Monster) ||
            (obj1->Type == EGameObjectType::Monster && obj2->Type == EGameObjectType::Attack))
        {
            UAttackObject* attack = nullptr;
            UMonster* monster = nullptr;
            
            if (obj1->Type == EGameObjectType::Attack)
            {
                attack = static_cast<UAttackObject*>(obj1);
                monster = static_cast<UMonster*>(obj2);
            }
            else
            {
                attack = static_cast<UAttackObject*>(obj2);
                monster = static_cast<UMonster*>(obj1);
            }
            
            // 몬스터에게 데미지
            monster->TakeDamage(attack->Damage);
            
            // 공격 오브젝트 제거
            attack->bIsActive = false;
        }
    }
    
    void CreateAttack(float x, float y, float damage, UGameObject* owner, float direction)
    {
        UAttackObject* attack = new UAttackObject(x, y, damage, owner, direction);
        AddGameObject(attack);
    }
    
    std::vector<UMonster*> GetMonsters()
    {
        std::vector<UMonster*> monsters;
        for (auto obj : GameObjects)
        {
            if (obj->Type == EGameObjectType::Monster && obj->bIsActive)
            {
                monsters.push_back(static_cast<UMonster*>(obj));
            }
        }
        return monsters;
    }
};

// WIC 로더: 파일을 32bpp BGRA로 변환해 텍스처/ SRV 생성
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
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;  // BGRA 포맷으로 알파 채널 포함
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
    renderer.CreateBlendState();  // 투명도 처리를 위한 블렌드 상태 생성
    
    // 플레이어 텍스처 로드
    if (FAILED(CreateTextureFromFileDDS(renderer.Device, L"Sprites/Character.dds", &renderer.TextureSRV)))
    {
        // 폴백: 기존 WIC 로더
        CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Character.jpg", &renderer.TextureSRV);
    }
    
    // 몬스터 텍스처 로드 (PNG 투명도 지원)
    CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Monster.png", &renderer.MonsterTextureSRV);
    
    // 스킬 텍스처 로드 (공격 이펙트용)
    CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Skill.gif", &renderer.SkillTextureSRV);

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

    renderer.Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &renderer.VertexBuffer);

    // 게임 오브젝트 관리자 생성
    UGameObjectManager gameManager;
    
    // 카메라 생성
    FCamera camera;
    
    // 플레이어 생성
    UPlayer* player = new UPlayer(0.0f, 0.0f);
    player->GroundY = 0.0f;
    player->bOnGround = true;
    gameManager.AddGameObject(player);
    
    // 몬스터 생성
    UMonster* monster1 = new UMonster(2.0f, 0.0f);
    UMonster* monster2 = new UMonster(-2.0f, 0.0f);
    UMonster* monster3 = new UMonster(4.0f, 0.0f);
    UMonster* monster4 = new UMonster(-4.0f, 0.0f);
    gameManager.AddGameObject(monster1);
    gameManager.AddGameObject(monster2);
    gameManager.AddGameObject(monster3);
    gameManager.AddGameObject(monster4);

    // 게임 상수
    const float gravity = -2.5f;   // 초당 중력 가속
    const float jumpSpeed = 1.6f;  // 점프 초기 속도
    const float moveSpeed = 1.6f;  // 좌우 이동 속도
    const float attackDamage = 25.0f; // 공격 데미지

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
        SHORT left  = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');
        SHORT right = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');
        SHORT jump  = GetAsyncKeyState(VK_SPACE);
        SHORT attack = GetAsyncKeyState('Z'); // Z키로 공격

        // 플레이어 이동 처리
        if (left & 0x8000)  player->MoveLeft(dt);
        if (right & 0x8000) player->MoveRight(dt);

        // 점프 처리
        if (jump & 0x8000) player->Jump();

        // 공격 처리
        if ((attack & 0x8000) && player->CanAttack())
        {
            // 플레이어가 바라보는 방향으로 공격
            float attackDirection = player->GetAttackDirection();
            gameManager.CreateAttack(player->PositionX, player->PositionY, attackDamage, player, attackDirection);
            player->StartAttackCooldown();
        }

        // 카메라가 플레이어를 따라가도록 업데이트
        camera.FollowPlayer(player->PositionX, player->PositionY);

        // 게임 오브젝트 업데이트
        gameManager.Update(dt);

        // 준비 작업
        renderer.Prepare();
        renderer.PrepareShader();

        // 게임 오브젝트들 렌더링
        gameManager.Render(renderer, camera);
        
        // 현재 화면에 보여지는 버퍼와 그리기 작업을 위한 버퍼를 서로 교환
        renderer.SwapBuffer();

	}

    ///////////////////////////////////////////////
	// 소멸하는 코드를 여기에 추가.

    // D3D11 소멸 시키는 함수를 호출.

    if (renderer.VertexBuffer) { renderer.VertexBuffer->Release(); renderer.VertexBuffer = nullptr; }
    renderer.ReleaseTexture();
    renderer.ReleaseShader();
    renderer.Release();
    if (renderer.ConstantBuffer) { renderer.ConstantBuffer->Release(); renderer.ConstantBuffer = nullptr; }

    CoUninitialize();

	return 0;
}
