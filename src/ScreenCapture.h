#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <opencv2/core.hpp>
#include <vector>
struct MonitorInfo { RECT rect{}; bool primary=false; std::wstring name; };
class IScreenCapture { public: virtual ~IScreenCapture()=default; virtual bool Capture(cv::Mat&)=0; virtual SIZE Size()const=0; };
class DxgiScreenCapture final:public IScreenCapture { public: explicit DxgiScreenCapture(int); bool Capture(cv::Mat&)override; SIZE Size()const override{return size_;} static std::vector<MonitorInfo> ListMonitors(); private: bool Initialize(); int index_;SIZE size_{}; Microsoft::WRL::ComPtr<ID3D11Device> device_;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_; };
