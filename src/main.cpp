// Base code from https://www.youtube.com/watch?v=aY8MNCNN-cY

// Windows API
#include <Windows.h>

// Used to create lists
#include <vector>

// Desktop Window Manager - Controls all windows on the desktop
#include <dwmapi.h>

// DirectX 11
#include <d3d11.h>

// Dear ImGUI
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

// Snapping Util
#include "snapping.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param)) {
		return 0L;
	}

	if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0L;
	}

	return DefWindowProc(window, message, w_param, l_param);
}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show) {
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = L"Whiteboard Class";

	RegisterClassExW(&wc);

	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);

	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT,  // Adding ` | WS_EX_LAYERED` allows clicks to fall through to applications underneath
		wc.lpszClassName,
		L"Whiteboard",
		WS_POPUP,
		0,
		0,
		desktop.right * 2,
		desktop.bottom,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	{
		RECT client_area{};
		GetClientRect(window, &client_area);

		RECT window_area{};
		GetWindowRect(window, &window_area);

		POINT diff{};
		ClientToScreen(window, &diff);

		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),  // No clue why this isn't just set to diff.x
			window_area.top + (diff.y - window_area.top),    // No clue why this isn't just set to diff.y
			client_area.right * 2,
			client_area.bottom
		};

		DwmExtendFrameIntoClientArea(window, &margins);
	}

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};

	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer) {
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	} else {
		return 1;
	}

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);
	ImGuiIO& io = ImGui::GetIO();

	bool running = true;

	int placeInHistory = 0;
	std::vector<ImVector<ImVec2>> history;
	ImVector<ImVec2> drawn;
	history.push_back(drawn);
	bool drawingLine = false;
	bool undoPrevFrame = false;
	bool redoNextFrame = false;
	bool drawingStraightLine = false;
	ImVec2 lineStart;

	while (running) {
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				running = false;
			} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
				running = false;
			}

			if (!running) {
				break;
			}

			// Starting New Frame
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();

			ImGui::NewFrame();

			// Preparing Rendering
			// ImGui::GetBackgroundDrawList()->AddRectFilled({ 0, 0 }, { (float) desktop.right * 2.f, (float) desktop.bottom }, ImColor(0, 0, 0, 180));
			//ImGui::GetBackgroundDrawList()->AddCircleFilled({500, 500}, 10.f, ImColor(1.f, 0.f, 0.f));
			ImGui::GetBackgroundDrawList()->AddText({10, 10}, IM_COL32(255, 255, 255, 255), "Whiteboard Open");

			ImVec2 mouse_pos = io.MousePos;
			if (io.MouseDown[0] && !drawingStraightLine) {
				drawingLine = true;

				bool inList = false;
				for (ImVec2 point : drawn) {
					if (point.x == mouse_pos.x && point.y == mouse_pos.y) {
						inList = true;
						break;
					}
				}
				if (!inList) drawn.push_back(mouse_pos);
			} else if (drawingLine) {
				drawingLine = false;
				drawn.push_back({-1, -1});
				if (placeInHistory != history.size() - 1) {
					history.erase(history.begin() + placeInHistory + 1, history.end());
				}
				placeInHistory++;
				history.push_back(drawn);
			} else {
				if (io.MouseClicked[4] && !redoNextFrame && !drawingStraightLine) {
					if (placeInHistory < history.size() - 1) placeInHistory++;
					drawn = history[placeInHistory];
				} else if (redoNextFrame) {
					redoNextFrame = false;
				} else if (io.MouseClicked[3] && !undoPrevFrame && !drawingStraightLine) {
					if (placeInHistory >= 1) placeInHistory--;
					drawn = history[placeInHistory];
				} else if (undoPrevFrame) {
					undoPrevFrame = false;
				} else if (io.MouseDown[1]) {
					if (!drawingStraightLine) {
						drawingStraightLine = true;
						lineStart = mouse_pos;
					}
				} else if (drawingStraightLine) {
					drawingStraightLine = false;
					drawn.push_back(lineStart);
					if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift))) {
						drawn.push_back(snap(lineStart, mouse_pos));
					} else {
						drawn.push_back(mouse_pos);
					}
					drawn.push_back({ -1, -1 });
					if (placeInHistory != history.size() - 1) {
						history.erase(history.begin() + placeInHistory + 1, history.end());
					}
					placeInHistory++;
					history.push_back(drawn);
				}
			}

			for (int i = 1; i < drawn.Size; i++) {
				ImVec2 point1 = drawn[i - 1];
				ImVec2 point2 = drawn[i];
				if (point1.x == -1 || point2.x == -1) continue;
				ImGui::GetBackgroundDrawList()->AddLine(point1, point2, IM_COL32(255, 0, 0, 255), 3.f);
			}

			if (drawingStraightLine) {
				if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift))) {
					ImGui::GetBackgroundDrawList()->AddLine(lineStart, snap(lineStart, mouse_pos), IM_COL32(255, 0, 0, 255), 3.f);
				} else {
					ImGui::GetBackgroundDrawList()->AddLine(lineStart, mouse_pos, IM_COL32(255, 0, 0, 255), 3.f);
				}
			}

			// Rendering
			ImGui::Render();

			constexpr float color[4]{ 0.f, 0.f, 0.f, 0.f };
			device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
			device_context->ClearRenderTargetView(render_target_view, color);

			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			swap_chain->Present(1U, 0U);  // First arg -> 1U = VSync, 0U = Not Vsync
		}
	}

	// Closing program
	drawn.clear();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	if (swap_chain) swap_chain->Release();
	if (device_context) device_context->Release();
	if (device) device->Release();
	if (render_target_view) render_target_view->Release();

	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}
