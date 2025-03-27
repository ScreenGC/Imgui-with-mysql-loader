#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <mysql.h>
#include "includes/guidev.h"
#include "includes/theme.h"
#include "imgui/imgui_impl_win32.h"
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")


bool reset_size = true;
bool ncmenu = true;
int tabs = 1;
Theme theme;


char username[64] = "";
char password[64] = "";
bool logged_in = false;
std::string login_message = "";


std::string getPublicIP() {
    HINTERNET hInternet, hConnect;
    DWORD bytesRead;
    char buffer[128];


    hInternet = InternetOpenA("GetPublicIP", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);  
    if (hInternet == NULL) {
        return "Unknown";
    }


    hConnect = InternetOpenUrlA(hInternet, "http://api.ipify.org", NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect == NULL) {
        InternetCloseHandle(hInternet);
        return "Unknown";
    }

    if (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) == 0) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "Unknown";
    }

    buffer[bytesRead] = '\0';
    std::string publicIP(buffer);

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return publicIP;
}


void login(MYSQL* conn) {

    std::string query = "SELECT * FROM users WHERE username = '" + std::string(username) + "' AND password = '" + std::string(password) + "' LIMIT 1";

    if (mysql_query(conn, query.c_str())) {
        login_message = "Query failed: " + std::string(mysql_error(conn));
        return;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        login_message = "Store result failed: " + std::string(mysql_error(conn));
        return;
    }

    if (mysql_num_rows(res) > 0) {
        MYSQL_ROW row = mysql_fetch_row(res);

        int blocked = atoi(row[4]); 
        if (blocked == 1) {
            login_message = "Você está bloqueado!";
            mysql_free_result(res);
            return;
        }

        std::string storedIP = row[6];  
        int firstAccess = atoi(row[5]);  
        std::string userKey = row[3];  


        if (userKey == "0") {
            login_message = "Login negado, chave inválida.";
            mysql_free_result(res);
            return;
        }


        if (userKey.empty()) {
            login_message = "Chave inválida, ative no site.";
            mysql_free_result(res);
            return;
        }


        std::string keyQuery = "SELECT * FROM `user_keys` WHERE key_value = '" + userKey + "' AND used = FALSE LIMIT 1";
        if (mysql_query(conn, keyQuery.c_str())) {
            login_message = "Query failed: " + std::string(mysql_error(conn));
            mysql_free_result(res);
            return;
        }

        MYSQL_RES* keyRes = mysql_store_result(conn);
        if (!keyRes) {
            login_message = "Store result failed: " + std::string(mysql_error(conn));
            mysql_free_result(res);
            return;
        }


        if (firstAccess == 1) {

            std::string updateIPQuery = "UPDATE users SET ip_address = '" + getPublicIP() + "', first_access = 0 WHERE username = '" + std::string(username) + "'";
            if (mysql_query(conn, updateIPQuery.c_str())) {
                login_message = "Erro ao atualizar o IP no banco de dados: " + std::string(mysql_error(conn));
                mysql_free_result(keyRes);
                mysql_free_result(res);
                return;
            }
            login_message = "IP registrado com sucesso e primeira vez de login concluída.";
        }
        else {

            std::string publicIP = getPublicIP();
            if (publicIP != storedIP) {
                login_message = "IP público não corresponde ao registrado.";
                mysql_free_result(keyRes);
                mysql_free_result(res);
                return;
            }
        }


        logged_in = true;
        login_message = "Login bem-sucedido!";

        mysql_free_result(keyRes);
    }
    else {
        login_message = "Usuário ou senha inválidos.";
    }

    mysql_free_result(res);
}


int main()
{
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(console, 1);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("CarrotLoader"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("CarrotLoader"), WS_OVERLAPPEDWINDOW, 0, 0, 50, 50, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(GetConsoleWindow(), SW_HIDE);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    theme.Register(io);

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 4.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    bool done = true;
    std::string str_window_title = "Carrot Loader";
    const char* window_title = str_window_title.c_str();
    ImVec2 window_size{ 450, 200 };
    DWORD window_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;

    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, "DatabaseIP", "User", "Pass", "DB", 3306, NULL, 0)) {
        login_message = "Erro ao conectar ao banco de dados.";
    }

    while (done)
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = false;
        }
        if (!done)
            break;

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (ncmenu)
        {
            if (reset_size) {
                ImGui::SetNextWindowSize(window_size);
                reset_size = false;
            }
            ImGui::SetNextWindowBgAlpha(1.0f);
            ImGui::PushFont(theme.font);
            ImGui::Begin(window_title, &ncmenu, window_flags);

            if (!logged_in) {
                ImGui::Text("Login");
                ImGui::InputText("Usuário", username, IM_ARRAYSIZE(username));
                ImGui::InputText("Senha", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);
                if (ImGui::Button("Entrar")) {
                    login(conn);
                }
                ImGui::Text("%s", login_message.c_str());



            }
            else {
                ImGui::Text("Bem-vindo, %s!", username);
            }

            ImGui::PopFont();
            ImGui::End();
        }
        else
        {
            exit(0);
        }

        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, NULL, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    mysql_close(conn);
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
