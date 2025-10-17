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

// �ؽ�ó �簢�� ����
struct FVertexTex
{
    float x, y, z;    // Position (NDC)
    float u, v;       // UV
};

// URenderer Ŭ���� (���� ������Ʈ�麸�� �տ� ����)
class URenderer
{
public:
    // Direct3D 11 ��ġ(Device)�� ��ġ ���ؽ�Ʈ(Device Context) �� ���� ü��(Swap Chain)�� �����ϱ� ���� �����͵�
    ID3D11Device* Device = nullptr; // GPU�� ����ϱ� ���� Direct3D ��ġ
    ID3D11DeviceContext* DeviceContext = nullptr; // GPU ��� ������ ����ϴ� ���ؽ�Ʈ
    IDXGISwapChain* SwapChain = nullptr; // ������ ���۸� ��ü�ϴ� �� ���Ǵ� ���� ü��

    // �������� �ʿ��� ���ҽ� �� ���¸� �����ϱ� ���� ������
    ID3D11Texture2D* FrameBuffer = nullptr; // ȭ�� ��¿� �ؽ�ó
    ID3D11RenderTargetView* FrameBufferRTV = nullptr; // �ؽ�ó�� ���� Ÿ������ ����ϴ� ��
    ID3D11RasterizerState* RasterizerState = nullptr; // �����Ͷ����� ����(�ø�, ä��� ��� �� ����)
    ID3D11Buffer* ConstantBuffer = nullptr; // ���̴��� �����͸� �����ϱ� ���� ��� ����

    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // ȭ���� �ʱ�ȭ(clear)�� �� ����� ���� (RGBA)
    D3D11_VIEWPORT ViewportInfo; // ������ ������ �����ϴ� ����Ʈ ����

    ID3D11VertexShader* SimpleVertexShader;
    ID3D11PixelShader* SimplePixelShader;
    ID3D11InputLayout* SimpleInputLayout;
    unsigned int Stride;

    // �ؽ�ó �������� �ʿ��� ���ҽ�
    ID3D11ShaderResourceView* TextureSRV = nullptr;  // �÷��̾� �ؽ�ó
    ID3D11ShaderResourceView* MonsterTextureSRV = nullptr;  // ���� �ؽ�ó
    ID3D11ShaderResourceView* SkillTextureSRV = nullptr;  // ��ų �ؽ�ó
    ID3D11SamplerState* TextureSampler = nullptr;
    ID3D11BlendState* BlendState = nullptr;  // ���� ó���� ���� ���� ����
    
    // ���� ���ؽ� ����
    ID3D11Buffer* VertexBuffer = nullptr;

public:
    // ������ �ʱ�ȭ �Լ�
    void Create(HWND hWindow)
    {
        // Direct3D ��ġ �� ���� ü�� ����
        CreateDeviceAndSwapChain(hWindow);

        // ������ ���� ����
        CreateFrameBuffer();

        // �����Ͷ����� ���� ����
        CreateRasterizerState();

        // ���� ���ٽ� ���� �� ���� ���´� �� �ڵ忡���� �ٷ��� ����
    }

    // Direct3D ��ġ �� ���� ü���� �����ϴ� �Լ�
    void CreateDeviceAndSwapChain(HWND hWindow)
    {
        // �����ϴ� Direct3D ��� ������ ����
        D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

        // ���� ü�� ���� ����ü �ʱ�ȭ
        DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
        swapchaindesc.BufferDesc.Width = 0; // â ũ�⿡ �°� �ڵ����� ����
        swapchaindesc.BufferDesc.Height = 0; // â ũ�⿡ �°� �ڵ����� ����
        swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // ���� ����
        swapchaindesc.SampleDesc.Count = 1; // ��Ƽ ���ø� ��Ȱ��ȭ
        swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // ���� Ÿ������ ���
        swapchaindesc.BufferCount = 2; // ���� ���۸�
        swapchaindesc.OutputWindow = hWindow; // �������� â �ڵ�
        swapchaindesc.Windowed = TRUE; // â ���
        swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // ���� ���

        // Direct3D ��ġ�� ���� ü���� ����
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
            featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
            &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

        // ������ ���� ü���� ���� ��������
        SwapChain->GetDesc(&swapchaindesc);

        // ����Ʈ ���� ����
        ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
    }

    // Direct3D ��ġ �� ���� ü���� �����ϴ� �Լ�
    void ReleaseDeviceAndSwapChain()
    {
        if (DeviceContext)
        {
            DeviceContext->Flush(); // �����ִ� GPU ��� ����
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

    // ������ ���۸� �����ϴ� �Լ�
    void CreateFrameBuffer()
    {
        // ���� ü�����κ��� �� ���� �ؽ�ó ��������
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

        // ���� Ÿ�� �� ����
        D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
        framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // ���� ����
        framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D �ؽ�ó

        Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
    }

    // ������ ���۸� �����ϴ� �Լ�
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

    // �����Ͷ����� ���¸� �����ϴ� �Լ�
    void CreateRasterizerState()
    {
        D3D11_RASTERIZER_DESC rasterizerdesc = {};
        rasterizerdesc.FillMode = D3D11_FILL_SOLID; // ä��� ���
        rasterizerdesc.CullMode = D3D11_CULL_NONE; // Ǯ��ũ�� �簢���� �ø� ��Ȱ��ȭ�� ����

        Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
    }

    // �����Ͷ����� ���¸� �����ϴ� �Լ�
    void ReleaseRasterizerState()
    {
        if (RasterizerState)
        {
            RasterizerState->Release();
            RasterizerState = nullptr;
        }
    }

    // �������� ���� ��� ���ҽ��� �����ϴ� �Լ�
    void Release()
    {
        RasterizerState->Release();

        // ���� Ÿ���� �ʱ�ȭ
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

        ReleaseFrameBuffer();
        ReleaseDeviceAndSwapChain();
    }

    // ���� ü���� �� ���ۿ� ����Ʈ ���۸� ��ü�Ͽ� ȭ�鿡 ���
    void SwapBuffer()
    {
        SwapChain->Present(1, 0); // 1: VSync Ȱ��ȭ
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
        
        // ���� ó���� ���� ���� ���� ����
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

// ī�޶� �ý���
struct FCamera
{
    float PositionX;  // ī�޶� ���� X ��ġ
    float PositionY;  // ī�޶� ���� Y ��ġ
    float ViewWidth;  // ����Ʈ �ʺ�
    float ViewHeight; // ����Ʈ ����
    
    FCamera() : PositionX(0.0f), PositionY(0.0f), ViewWidth(4.0f), ViewHeight(3.0f) {}
    
    // ���� ��ǥ�� ȭ�� ��ǥ�� ��ȯ
    void WorldToScreen(float worldX, float worldY, float& screenX, float& screenY)
    {
        screenX = (worldX - PositionX) / (ViewWidth * 0.5f);
        screenY = (worldY - PositionY) / (ViewHeight * 0.5f);
    }
    
    // ȭ�� ��ǥ�� ���� ��ǥ�� ��ȯ
    void ScreenToWorld(float screenX, float screenY, float& worldX, float& worldY)
    {
        worldX = screenX * (ViewWidth * 0.5f) + PositionX;
        worldY = screenY * (ViewHeight * 0.5f) + PositionY;
    }
    
    // �÷��̾ ���󰡵��� ī�޶� ��ġ ������Ʈ
    void FollowPlayer(float playerX, float playerY)
    {
        // �ε巯�� ī�޶� �̵� (���� ����)
        float targetX = playerX;
        float targetY = playerY;
        
        PositionX += (targetX - PositionX) * 0.1f;
        PositionY += (targetY - PositionY) * 0.1f;
    }
};

// �ݶ��̴� ����ü
struct FCollider
{
    float centerX, centerY;  // �߽���
    float halfWidth, halfHeight;  // ����, �ݳ���
    
    FCollider(float x = 0.0f, float y = 0.0f, float w = 0.0f, float h = 0.0f)
        : centerX(x), centerY(y), halfWidth(w), halfHeight(h) {}
    
    // AABB �浹 ����
    bool CheckCollision(const FCollider& other) const
    {
        return (abs(centerX - other.centerX) < (halfWidth + other.halfWidth)) &&
               (abs(centerY - other.centerY) < (halfHeight + other.halfHeight));
    }
    
    // ��ġ ������Ʈ
    void UpdatePosition(float x, float y)
    {
        centerX = x;
        centerY = y;
    }
};

// ���� ������Ʈ Ÿ��
enum class EGameObjectType
{
    Player,
    Monster,
    Attack,
};

// �⺻ ���� ������Ʈ Ŭ����
class UGameObject
{
public:
    EGameObjectType Type;
    float PositionX, PositionY;  // ���� ��ǥ
    float ScaleX, ScaleY;
    FCollider Collider;
    bool bIsActive;
    bool bFacingRight;  // �������� ���� �ִ��� (���� ��Ÿ��)
    
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
    
    // ���� ��ȯ
    void SetFacingRight(bool facingRight)
    {
        bFacingRight = facingRight;
    }
};

// ����ƺ� ���� Ŭ����
class UMonster : public UGameObject
{
public:
    float Health;
    float MaxHealth;
    bool bIsAlive;
    float MoveSpeed;
    float Direction;  // �̵� ���� (-1 �Ǵ� 1)
    
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
        
        // ��Ʈ ������ �¿�� �̵�
        PositionX += Direction * MoveSpeed * deltaTime;
        
        // ȭ�� ��迡�� ���� ��ȯ
        if (PositionX < -2.0f || PositionX > 2.0f)
        {
            Direction *= -1.0f;
            bFacingRight = Direction > 0.0f;
        }
        
        UpdateCollider();
    }
    
    void Render(URenderer& renderer, FCamera& camera) override;
};

// ���� ������Ʈ Ŭ����
class UAttackObject : public UGameObject
{
public:
    float Damage;
    float LifeTime;
    float CurrentLifeTime;
    UGameObject* Owner;
    float AttackRange;
    float AttackDirection;  // ���� ���� (-1 �Ǵ� 1)
    
    UAttackObject(float x, float y, float damage, UGameObject* owner, float direction)
        : UGameObject(EGameObjectType::Attack, x, y, 0.3f, 0.3f)
        , Damage(damage), LifeTime(0.2f), CurrentLifeTime(0.0f), Owner(owner)
        , AttackRange(0.8f), AttackDirection(direction)
    {
        // ���� ���⿡ ���� ��ġ ����
        PositionX += AttackDirection * AttackRange * 0.5f;
        UpdateCollider();
    }
    
    void Update(float deltaTime) override
    {
        CurrentLifeTime += deltaTime;
        
        // ���� �ִϸ��̼� ȿ�� (ũ�� ��ȭ)
        float progress = CurrentLifeTime / LifeTime;
        float scaleMultiplier = 1.0f + sin(progress * 3.14159f * 2.0f) * 0.3f;
        ScaleX = 0.15f * scaleMultiplier;
        ScaleY = 0.15f * scaleMultiplier;
        
        // ������ ����Ǹ鼭 ������ �̵�
        PositionX += AttackDirection * 1.5f * deltaTime;
        
        if (CurrentLifeTime >= LifeTime)
        {
            bIsActive = false;
        }
        UpdateCollider();
    }
    
    void Render(URenderer& renderer, FCamera& camera) override;
};

// �÷��̾� Ŭ����
class UPlayer : public UGameObject
{
public:
    float VelocityY;
    float AttackCooldown;
    float CurrentAttackCooldown;
    float MoveSpeed;
    bool bOnGround;
    float GroundY;  // �ٴ� Y ��ǥ
    
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
        
        // �߷� ����
        if (!bOnGround)
        {
            VelocityY -= 8.0f * deltaTime;  // �߷�
            PositionY += VelocityY * deltaTime;
            
            // �ٴڿ� ����
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


// UGameObject Render �Լ� ����
void UGameObject::Render(URenderer& renderer, FCamera& camera)
{
    // �⺻ ������ �������
}

void UMonster::Render(URenderer& renderer, FCamera& camera)
{
    if (!bIsActive) return;
    
    // ���� ��ǥ�� ȭ�� ��ǥ�� ��ȯ
    float screenX, screenY;
    camera.WorldToScreen(PositionX, PositionY, screenX, screenY);
    
    // ��� ���� ������Ʈ
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* cb = (float*)mapped.pData;
        cb[0] = screenX; cb[1] = screenY; cb[2] = ScaleX; cb[3] = ScaleY;
        renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
    }
    
    // ���� ���� �ؽ�ó ���
    if (renderer.MonsterTextureSRV)
    {
        renderer.DeviceContext->PSSetShaderResources(0, 1, &renderer.MonsterTextureSRV);
    }
    
    // ������
    renderer.RenderPrimitive(renderer.VertexBuffer, 6);
}

void UAttackObject::Render(URenderer& renderer, FCamera& camera)
{
    if (!bIsActive) return;
    
    // ���� ��ǥ�� ȭ�� ��ǥ�� ��ȯ
    float screenX, screenY;
    camera.WorldToScreen(PositionX, PositionY, screenX, screenY);
    
    // ��� ���� ������Ʈ
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* cb = (float*)mapped.pData;
        cb[0] = screenX; cb[1] = screenY; cb[2] = ScaleX; cb[3] = ScaleY;
        renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
    }
    
    // ��ų �ؽ�ó ��� (Skill.gif)
    if (renderer.SkillTextureSRV)
    {
        renderer.DeviceContext->PSSetShaderResources(0, 1, &renderer.SkillTextureSRV);
    }
    
    renderer.RenderPrimitive(renderer.VertexBuffer, 6);
}

void UPlayer::Render(URenderer& renderer, FCamera& camera)
{
    // ���� ��ǥ�� ȭ�� ��ǥ�� ��ȯ
    float screenX, screenY;
    camera.WorldToScreen(PositionX, PositionY, screenX, screenY);
    
    // ��� ���� ������Ʈ
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(renderer.DeviceContext->Map(renderer.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* cb = (float*)mapped.pData;
        cb[0] = screenX; cb[1] = screenY; cb[2] = ScaleX; cb[3] = ScaleY;
        renderer.DeviceContext->Unmap(renderer.ConstantBuffer, 0);
    }
    
    // �÷��̾� �ؽ�ó ���
    if (renderer.TextureSRV)
    {
        renderer.DeviceContext->PSSetShaderResources(0, 1, &renderer.TextureSRV);
    }
    
    renderer.RenderPrimitive(renderer.VertexBuffer, 6);
}

// ���� ������Ʈ ������
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
        // ��Ȱ�� ������Ʈ ����
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
        
        // �浹 ó��
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
                
                // �浹 ����
                if (obj1->Collider.CheckCollision(obj2->Collider))
                {
                    HandleCollision(obj1, obj2);
                }
            }
        }
    }
    
    void HandleCollision(UGameObject* obj1, UGameObject* obj2)
    {
        // ���� ������Ʈ�� ���� �浹
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
            
            // ���Ϳ��� ������
            monster->TakeDamage(attack->Damage);
            
            // ���� ������Ʈ ����
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

// WIC �δ�: ������ 32bpp BGRA�� ��ȯ�� �ؽ�ó/ SRV ����
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
    texDesc.MipLevels = 0; // ��ü ��ü�� �ڵ� ����
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;  // BGRA �������� ���� ä�� ����
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* texture = nullptr;
    // �� �ڵ� ������ ���� �ʱ� ������ ���� ���� ��, ����0�� UpdateSubresource�� ä��
    hr = device->CreateTexture2D(&texDesc, nullptr, &texture);
    if (SUCCEEDED(hr))
    {
        // ����0 ���ε�
        context->UpdateSubresource(texture, 0, nullptr, pixels, stride, 0);

        // SRV ���� (��ü �� ���)
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = -1; // ��� ��
        hr = device->CreateShaderResourceView(texture, &srvDesc, outSRV);
        if (SUCCEEDED(hr))
        {
            // �Ӹ� ����
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

// ������ DDS �δ�: DXT1 / DXT5 (BC1/BC3, sRGB)�� ����, 2D �ؽ�ó/���Է� ���
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

    // ���� ���̳ʸ� ��ü �б�
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

    // ���긮�ҽ� ����
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

// ���� �޽����� ó���� �Լ�
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
	// ������ Ŭ���� �̸�
	WCHAR WindowClass[] = L"WindowClass";

	// ������ Ÿ��Ʋ�ٿ� ǥ�õ� �̸�
	WCHAR Title[] = L"My Window";

	// ���� �޽����� ó���� �Լ��� WndProc�� �Լ� �����͸� WindowClass ����ü�� �ִ´�.
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	// ������ Ŭ���� ���
	RegisterClassW(&wndclass);

	// 1024 x 1024 ũ�⿡ ������ ����
	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);

	bool bIsExit = false;

    ///////////////////////////////////////////////
	// ���� �����ϴ� �ڵ带 ���⿡ �߰�.

    // COM �ʱ�ȭ (WIC ���)
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Renderer Class�� ����.
    URenderer renderer;

    // D3D11 �����ϴ� �Լ��� ȣ��.
    renderer.Create(hWnd);
    renderer.CreateShader();

    // �ؽ�ó �ε� �� ���÷� ����
    renderer.CreateTextureSampler();
    renderer.CreateBlendState();  // ���� ó���� ���� ���� ���� ����
    
    // �÷��̾� �ؽ�ó �ε�
    if (FAILED(CreateTextureFromFileDDS(renderer.Device, L"Sprites/Character.dds", &renderer.TextureSRV)))
    {
        // ����: ���� WIC �δ�
        CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Character.jpg", &renderer.TextureSRV);
    }
    
    // ���� �ؽ�ó �ε� (PNG ���� ����)
    CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Monster.png", &renderer.MonsterTextureSRV);
    
    // ��ų �ؽ�ó �ε� (���� ����Ʈ��)
    CreateTextureFromFileWIC(renderer.Device, renderer.DeviceContext, L"Sprites/Skill.gif", &renderer.SkillTextureSRV);

    // ��� ���� ���� (Offset.xy, Scale.xy)
    {
        D3D11_BUFFER_DESC cbd{};
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.ByteWidth = sizeof(float) * 4;
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        renderer.Device->CreateBuffer(&cbd, nullptr, &renderer.ConstantBuffer);
    }

    // Ǯ��ũ�� �簢�� ���� ���� ���� (TRIANGLELIST 6�� ����)
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

    // ���� ������Ʈ ������ ����
    UGameObjectManager gameManager;
    
    // ī�޶� ����
    FCamera camera;
    
    // �÷��̾� ����
    UPlayer* player = new UPlayer(0.0f, 0.0f);
    player->GroundY = 0.0f;
    player->bOnGround = true;
    gameManager.AddGameObject(player);
    
    // ���� ����
    UMonster* monster1 = new UMonster(2.0f, 0.0f);
    UMonster* monster2 = new UMonster(-2.0f, 0.0f);
    UMonster* monster3 = new UMonster(4.0f, 0.0f);
    UMonster* monster4 = new UMonster(-4.0f, 0.0f);
    gameManager.AddGameObject(monster1);
    gameManager.AddGameObject(monster2);
    gameManager.AddGameObject(monster3);
    gameManager.AddGameObject(monster4);

    // ���� ���
    const float gravity = -2.5f;   // �ʴ� �߷� ����
    const float jumpSpeed = 1.6f;  // ���� �ʱ� �ӵ�
    const float moveSpeed = 1.6f;  // �¿� �̵� �ӵ�
    const float attackDamage = 25.0f; // ���� ������

    // �ð� ������
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    LARGE_INTEGER prev; QueryPerformanceCounter(&prev);

    // Main Loop (Quit Message�� ������ ������ �Ʒ� Loop�� ������ �����ϰ� ��)
	while (bIsExit == false)
	{
		MSG msg;

		// ó���� �޽����� �� �̻� ������ ���� ����
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			// Ű �Է� �޽����� ����
			TranslateMessage(&msg);

			// �޽����� ������ ������ ���ν����� ����, �޽����� ������ ����� WndProc ���� ���޵�
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}
        ///////////////////////////////////////////////
        // // �Ź� ����Ǵ� �ڵ带 ���⿡ �߰�.
  
        // deltaTime ���
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        float dt = float(now.QuadPart - prev.QuadPart) / float(freq.QuadPart);
        prev = now;

        // �Է� ó��
        SHORT left  = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');
        SHORT right = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');
        SHORT jump  = GetAsyncKeyState(VK_SPACE);
        SHORT attack = GetAsyncKeyState('Z'); // ZŰ�� ����

        // �÷��̾� �̵� ó��
        if (left & 0x8000)  player->MoveLeft(dt);
        if (right & 0x8000) player->MoveRight(dt);

        // ���� ó��
        if (jump & 0x8000) player->Jump();

        // ���� ó��
        if ((attack & 0x8000) && player->CanAttack())
        {
            // �÷��̾ �ٶ󺸴� �������� ����
            float attackDirection = player->GetAttackDirection();
            gameManager.CreateAttack(player->PositionX, player->PositionY, attackDamage, player, attackDirection);
            player->StartAttackCooldown();
        }

        // ī�޶� �÷��̾ ���󰡵��� ������Ʈ
        camera.FollowPlayer(player->PositionX, player->PositionY);

        // ���� ������Ʈ ������Ʈ
        gameManager.Update(dt);

        // �غ� �۾�
        renderer.Prepare();
        renderer.PrepareShader();

        // ���� ������Ʈ�� ������
        gameManager.Render(renderer, camera);
        
        // ���� ȭ�鿡 �������� ���ۿ� �׸��� �۾��� ���� ���۸� ���� ��ȯ
        renderer.SwapBuffer();

	}

    ///////////////////////////////////////////////
	// �Ҹ��ϴ� �ڵ带 ���⿡ �߰�.

    // D3D11 �Ҹ� ��Ű�� �Լ��� ȣ��.

    if (renderer.VertexBuffer) { renderer.VertexBuffer->Release(); renderer.VertexBuffer = nullptr; }
    renderer.ReleaseTexture();
    renderer.ReleaseShader();
    renderer.Release();
    if (renderer.ConstantBuffer) { renderer.ConstantBuffer->Release(); renderer.ConstantBuffer = nullptr; }

    CoUninitialize();

	return 0;
}
