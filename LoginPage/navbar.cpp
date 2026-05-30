#include <iostream>
#include <windows.h>
#include <string>

using namespace std;

// Renk kodları için fonksiyon
void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Üst bar çizme fonksiyonu
void drawNavbar() {
    setColor(7);  // Gri renk
    
    const int width = 120;  // Daha geniş bir bar
    
    // Üst çizgi
    cout << string(width, '=') << endl;
    
    // Boş alan
    cout << "|" << string(width-2, ' ') << "|" << endl;
    
    // Alt çizgi
    cout << string(width, '=') << endl;
}

int main() {
    // Türkçe karakter desteği
    SetConsoleOutputCP(65001);
    
    // Konsol başlığı
    SetConsoleTitle(L"Geniş Üst Bar");
    
    // Konsol boyutunu ayarla
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    SMALL_RECT windowSize = {0, 0, 120, 5};
    SetConsoleWindowInfo(console, TRUE, &windowSize);
    
    // Üst barı çiz
    drawNavbar();
    
    // Programı açık tut
    cout << "\nProgramı kapatmak için bir tuşa basın...";
    cin.get();
    
    return 0;
} 