// Standalone Windows non-Qt check; compile command is in adjacent provenance.
#include "ui/NpcDialogColoredText.h"
#include <algorithm>
#include <iostream>

int main()
{
    HDC dc=CreateCompatibleDC(nullptr);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=96;info.bmiHeader.biHeight=-96;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* memory=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&memory,nullptr,0);
    if(!dc || !bitmap || !memory)return 1;
    const auto oldBitmap=SelectObject(dc,bitmap);
    HFONT font=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Arial");
    const auto oldFont=SelectObject(dc,font);
    auto* pixels=static_cast<unsigned int*>(memory);std::fill_n(pixels,96*96,0xffffffffu);
    const int one=MeasureNpcSayDialogColoredTextHeight(dc,1,u8"가",true);
    const int three=MeasureNpcSayDialogColoredTextHeight(dc,1,u8"가나다",true);
    const bool boundaries=one>0 && three==3*one;
    SetBkMode(dc,TRANSPARENT);
    DrawNpcSayDialogColoredText(dc,RECT{10,10,80,80},u8"가나다\n^FF0000가",0,true);GdiFlush();
    bool ink=false,red=false,clipped=true;
    for(int y=0;y<96;++y)for(int x=0;x<96;++x){
        const bool changed=(pixels[y*96+x]&0xffffff)!=0xffffff;
        ink=ink || changed;
        red=red || (pixels[y*96+x]&0xffffff)==0xff0000;
        if((x<10 || x>=80 || y<10 || y>=80) && changed)clipped=false;
    }
    SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);
    DeleteObject(font);DeleteObject(bitmap);DeleteDC(dc);
    if(!boundaries || !ink || !red || !clipped){std::cerr<<"FAIL UTF8 boundaries/draw/color/clip\n";return 2;}
    std::cout<<"PASS Windows non-Qt UTF8 codepoint wrapping, wide draw, colored text and parent clip\n";
}
