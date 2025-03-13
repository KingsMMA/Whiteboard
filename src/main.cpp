#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

#include "snapping.h"
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <iterator>

using namespace std;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
* The window procedure receives the messages being sent to the window
* This is just like hey imgui, do you want to handle?  If not, pass the message along
*/
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

class MemoryState {
public:
	ImVector<ImVec2> drawn;
	map<int, ImU32> colours{};
	float drawingColour[4];
	float lastDrawingColour[4];
	map<int, float> thicknesses{};
	float drawingThickness;
	float lastDrawingThickness;

	MemoryState(const ImVector<ImVec2>& drawn, const std::map<int, ImU32>& colours, const float drawingColour[4], const float lastDrawingColour[4], const map<int, float> thicknesses, const float drawingThickness, const float lastDrawingThickness)
		: drawn(drawn), colours(colours), thicknesses(thicknesses), drawingThickness(drawingThickness), lastDrawingThickness(lastDrawingThickness) {
		std::copy(drawingColour, drawingColour + 4, this->drawingColour);
		std::copy(lastDrawingColour, lastDrawingColour + 4, this->lastDrawingColour);
	}
};

/**
* @parameter instance This window's instance
* @parameter _ The previous window's instance
* @parameter _ The command line arguments
* @parameter cmd_show Determines whether or not we want to show our window
*/
INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show) {
	// Create the window class
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = L"Whiteboard Class";

	// Register the window class
	RegisterClassExW(&wc);

	// Use the window class to create a window
	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT,
		wc.lpszClassName,
		L"Whiteboard",
		WS_POPUP,
		-2560,
		0,
		2560 * 2, // width - look into using GetSystemMetrics or smth
		1440, // height
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);
	
	SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);  // The BYTE(255) is the alpha value.

	// This is needed to make stuff render
	{
		RECT client_area{};
		GetClientRect(window, &client_area);

		RECT window_area{};
		GetWindowRect(window, &window_area);

		POINT diff{};
		ClientToScreen(window, &diff);

		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		DwmExtendFrameIntoClientArea(window, &margins);
	}

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 144U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Compile time array containg the DX11 features that we want
	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0,
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

	// Create the render target
	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer) {
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	} else return 1;

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes(3.f);

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);
	ImGuiIO& io = ImGui::GetIO();

	// Vars
	bool running = true;

	// Menu Vars
	bool menuOpen = false;
	bool menuActive = true;
	bool backgroundEnabled = false;
	float backgroundOpacity = 0.4f;
	float drawingColour[4] = { 1.f, 0.f, 0.f, 1.f };
	float lastDrawingColour[4];
	float drawingThickness = 3.f;
	float lastDrawingThickness = 3.f;
	copy(drawingColour, drawingColour + 4, lastDrawingColour);

	// Drawing Vars
	int placeInHistory = 0;
	vector<MemoryState> history;
	ImVector<ImVec2> drawn;
	map<int, ImU32> colours {};
	map<int, float> thicknesses {};
	history.push_back(MemoryState(drawn, colours, drawingColour, lastDrawingColour, thicknesses, drawingThickness, lastDrawingThickness));
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
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) running = false;
		if (ImGui::IsKeyPressed(ImGuiKey_LeftAlt)) menuOpen = !menuOpen;

		if (!running) break;

		// Frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Rendering
		if (backgroundEnabled) ImGui::GetBackgroundDrawList()->AddRectFilled({ 0, 0 }, { 2560 * 2, 1440 }, ImColor(0.f, 0.f, 0.f, backgroundOpacity));  // Background

		if (!ranges::equal(drawingColour, lastDrawingColour)) {
			colours[drawn.Size] = IM_COL32(
				lround(drawingColour[0] * 255),
				lround(drawingColour[1] * 255),
				lround(drawingColour[2] * 255),
				lround(drawingColour[3] * 255)
			);
			drawn.push_back({ -1, -1 });
			copy(drawingColour, drawingColour + 4, lastDrawingColour);
		}

		if (drawingThickness != lastDrawingThickness) {
			thicknesses[drawn.Size] = drawingThickness;
			drawn.push_back({ -1, -1 });
			lastDrawingThickness = drawingThickness;
		}

		ImGui::GetBackgroundDrawList()->AddText({ 10, 10 }, ImColor(1.f, 1.f, 1.f, 1.f), std::to_string(history.size()).c_str());

		ImVec2 mouse_pos = io.MousePos;
		if (!io.WantCaptureMouse) {
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
			}
			else if (drawingLine) {
				drawingLine = false;

				drawn.push_back({ -1, -1 });

				if (placeInHistory != history.size() - 1) {
					history.erase(history.begin() + placeInHistory + 1, history.end());
				}
				placeInHistory++;
				history.push_back(MemoryState(drawn, colours, drawingColour, lastDrawingColour, thicknesses, drawingThickness, lastDrawingThickness));
			}
			else {
				if (io.MouseClicked[4] && !redoNextFrame && !drawingStraightLine) {
					if (placeInHistory < history.size() - 1) placeInHistory++;
					MemoryState unloading = history[placeInHistory];
					drawn = unloading.drawn;
					colours = unloading.colours;
					std::memcpy(drawingColour, unloading.drawingColour, sizeof(drawingColour));
					std::memcpy(lastDrawingColour, unloading.lastDrawingColour, sizeof(lastDrawingColour));
					thicknesses = unloading.thicknesses;
					drawingThickness = unloading.drawingThickness;
					lastDrawingThickness = unloading.lastDrawingThickness;
				}
				else if (redoNextFrame) {
					redoNextFrame = false;
				}
				else if (io.MouseClicked[3] && !undoPrevFrame && !drawingStraightLine) {
					if (placeInHistory >= 1) placeInHistory--;
					MemoryState unloading = history[placeInHistory];
					drawn = unloading.drawn;
					colours = unloading.colours;
					std::memcpy(drawingColour, unloading.drawingColour, sizeof(drawingColour));
					std::memcpy(lastDrawingColour, unloading.lastDrawingColour, sizeof(lastDrawingColour));
					thicknesses = unloading.thicknesses;
					drawingThickness = unloading.drawingThickness;
					lastDrawingThickness = unloading.lastDrawingThickness;
				}
				else if (undoPrevFrame) {
					undoPrevFrame = false;
				}
				else if (io.MouseDown[1]) {
					if (!drawingStraightLine) {
						drawingStraightLine = true;
						lineStart = mouse_pos;
					}
				}
				else if (drawingStraightLine) {
					drawingStraightLine = false;
					drawn.push_back(lineStart);
					if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
						drawn.push_back(snap(lineStart, mouse_pos));
					}
					else {
						drawn.push_back(mouse_pos);
					}

					drawn.push_back({ -1, -1 });

					if (placeInHistory != history.size() - 1) {
						history.erase(history.begin() + placeInHistory + 1, history.end());
					}
					placeInHistory++;
					history.push_back(MemoryState(drawn, colours, drawingColour, lastDrawingColour, thicknesses, drawingThickness, lastDrawingThickness));
				}
			}
		}

		ImU32 currentColour = IM_COL32(255, 0, 0, 255);
		float currentThickness = 3.f;
		for (int i = 1; i < drawn.Size; i++) {
			ImVec2 point1 = drawn[i - 1];
			ImVec2 point2 = drawn[i];
			if (point1.x == -1 || point2.x == -1) {
				ImU32 newColour = colours[point2.x == -1 ? i : i - 1];
				if (newColour) currentColour = newColour;

				ImU32 newThickness = thicknesses[point2.x == -1 ? i : i - 1];
				if (newThickness) currentThickness = newThickness;
				continue;
			}
			ImGui::GetBackgroundDrawList()->AddLine(point1, point2, currentColour, currentThickness);
		}

		if (drawingStraightLine) {
			if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
				ImGui::GetBackgroundDrawList()->AddLine(lineStart, snap(lineStart, mouse_pos), IM_COL32(
				lround(drawingColour[0] * 255),
				lround(drawingColour[1] * 255),
				lround(drawingColour[2] * 255),
				lround(drawingColour[3] * 255)
			), currentThickness);
			}
			else {
				ImGui::GetBackgroundDrawList()->AddLine(lineStart, mouse_pos, IM_COL32(
					lround(drawingColour[0] * 255),
					lround(drawingColour[1] * 255),
					lround(drawingColour[2] * 255),
					lround(drawingColour[3] * 255)
				), currentThickness);
			}
		}

		if (menuOpen) {
			ImGui::Begin("Settings", &menuActive);

			ImGui::Checkbox("Background", &backgroundEnabled);
			if (backgroundEnabled) {
				ImGui::SliderFloat("Background Opacity", &backgroundOpacity, 0.0f, 1.0f);
			}

			ImGui::Separator();

			ImGui::ColorEdit4("Drawing Colour", drawingColour);
			ImGui::SliderFloat("Line Thickness", &drawingThickness, 1.0f, 10.0f);

			ImGui::End();
		}

		// Finish Frame
		ImGui::Render();
		constexpr float colour[4]{ 0.f, 0.f, 0.f, 0.f };
		device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
		device_context->ClearRenderTargetView(render_target_view, colour);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		swap_chain->Present(1U, 0U);  // Vsync
	}

	// Close the program, cleanup
	// https://stackoverflow.com/a/10465032/12964643
	for (MemoryState memState : history) {
		memState.drawn.clear();
		memState.drawn.shrink(0);
		memState.colours.clear();
	}
	history.clear();
	history.shrink_to_fit();
	vector<MemoryState>().swap(history);

	drawn.clear();
	drawn.shrink(0);

	// Window cleanup
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
