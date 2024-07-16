#ifndef   __COLORS_H__
#define   __COLORS_H__

#include "ws2812_driver.h"

#define         AliceBlue              {0xF0,0xF8,0xFF}
#define         Amethyst               {0x99,0x66,0xCC}
#define         AntiqueWhite           {0xFA,0xEB,0xD7}
#define         Aqua                   {0x00,0xFF,0xFF}
#define         Aquamarine             {0x7F,0xFF,0xD4}
#define         Azure                  {0xF0,0xFF,0xFF}
#define         Beige                  {0xF5,0xF5,0xDC}
#define         Bisque                 {0xFF,0xE4,0xC4}
#define         Black                  {0x00,0x00,0x00}
#define         BlanchedAlmond         {0xFF,0xEB,0xCD}
#define         Blue                   {0x00,0x00,0xFF}
#define         BlueViolet             {0x8A,0x2B,0xE2}
#define         Brown                  {0xA5,0x2A,0x2A}
#define         BurlyWood              {0xDE,0xB8,0x87}
#define         CadetBlue              {0x5F,0x9E,0xA0}
#define         Chartreuse             {0x7F,0xFF,0x00}
#define         Chocolate              {0xD2,0x69,0x1E}
#define         Coral                  {0xFF,0x7F,0x50}
#define         CornflowerBlue         {0x64,0x95,0xED}
#define         Cornsilk               {0xFF,0xF8,0xDC}
#define         Crimson                {0xDC,0x14,0x3C}
#define         Cyan                   {0x00,0xFF,0xFF}
#define         DarkBlue               {0x00,0x00,0x8B}
#define         DarkCyan               {0x00,0x8B,0x8B}
#define         DarkGoldenrod          {0xB8,0x86,0x0B}
#define         DarkGreen              {0x00,0x64,0x00}
#define         DarkKhaki              {0xBD,0xB7,0x6B}
#define         DarkMagenta            {0x8B,0x00,0x8B}
#define         DarkOliveGreen         {0x55,0x6B,0x2F}
#define         DarkOrange             {0xFF,0x8C,0x00}
#define         DarkOrchid             {0x99,0x32,0xCC}
#define         DarkRed                {0x8B,0x00,0x00}
#define         DarkSalmon             {0xE9,0x96,0x7A}
#define         DarkSeaGreen           {0x8F,0xBC,0x8F}
#define         DarkSlateBlue          {0x48,0x3D,0x8B}
#define         DarkSlateGray          {0x2F,0x4F,0x4F}
#define         DarkSlateGrey          {0x2F,0x4F,0x4F}
#define         DarkTurquoise          {0x00,0xCE,0xD1}
#define         DarkViolet             {0x94,0x00,0xD3}
#define         DeepPink               {0xFF,0x14,0x93}
#define         DeepSkyBlue            {0x00,0xBF,0xFF}
#define         DimGray                {0x69,0x69,0x69}
#define         DimGrey                {0x69,0x69,0x69}
#define         DodgerBlue             {0x1E,0x90,0xFF}
#define         FireBrick              {0xB2,0x22,0x22}
#define         FloralWhite            {0xFF,0xFA,0xF0}
#define         ForestGreen            {0x22,0x8B,0x22}
#define         Fuchsia                {0xFF,0x00,0xFF}
#define         Gainsboro              {0xDC,0xDC,0xDC}
#define         GhostWhite             {0xF8,0xF8,0xFF}
#define         Gold                   {0xFF,0xD7,0x00}
#define         Goldenrod              {0xDA,0xA5,0x20}
#define         Gray                   {0x80,0x80,0x80}
#define         Grey                   {0x80,0x80,0x80}
#define         Green                  {0x00,0x80,0x00}
#define         GreenYellow            {0xAD,0xFF,0x2F}
#define         Honeydew               {0xF0,0xFF,0xF0}
#define         HotPink                {0xFF,0x69,0xB4}
#define         IndianRed              {0xCD,0x5C,0x5C}
#define         Indigo                 {0x4B,0x00,0x82}
#define         Ivory                  {0xFF,0xFF,0xF0}
#define         Khaki                  {0xF0,0xE6,0x8C}
#define         Lavender               {0xE6,0xE6,0xFA}
#define         LavenderBlush          {0xFF,0xF0,0xF5}
#define         LawnGreen              {0x7C,0xFC,0x00}
#define         LemonChiffon           {0xFF,0xFA,0xCD}
#define         LightBlue              {0xAD,0xD8,0xE6}
#define         LightCoral             {0xF0,0x80,0x80}
#define         LightCyan              {0xE0,0xFF,0xFF}
#define         LightGoldenrodYellow   {0xFA,0xFA,0xD2}
#define         LightGreen             {0x90,0xEE,0x90}
#define         LightGrey              {0xD3,0xD3,0xD3}
#define         LightPink              {0xFF,0xB6,0xC1}
#define         LightSalmon            {0xFF,0xA0,0x7A}
#define         LightSeaGreen          {0x20,0xB2,0xAA}
#define         LightSkyBlue           {0x87,0xCE,0xFA}
#define         LightSlateGray         {0x77,0x88,0x99}
#define         LightSlateGrey         {0x77,0x88,0x99}
#define         LightSteelBlue         {0xB0,0xC4,0xDE}
#define         LightYellow            {0xFF,0xFF,0xE0}
#define         Lime                   {0x00,0xFF,0x00}
#define         LimeGreen              {0x32,0xCD,0x32}
#define         Linen                  {0xFA,0xF0,0xE6}
#define         Magenta                {0xFF,0x00,0xFF}
#define         Maroon                 {0x80,0x00,0x00}
#define         MediumAquamarine       {0x66,0xCD,0xAA}
#define         MediumBlue             {0x00,0x00,0xCD}
#define         MediumOrchid           {0xBA,0x55,0xD3}
#define         MediumPurple           {0x93,0x70,0xDB}
#define         MediumSeaGreen         {0x3C,0xB3,0x71}
#define         MediumSlateBlue        {0x7B,0x68,0xEE}
#define         MediumSpringGreen      {0x00,0xFA,0x9A}
#define         MediumTurquoise        {0x48,0xD1,0xCC}
#define         MediumVioletRed        {0xC7,0x15,0x85}
#define         MediumWhite            {0x3F,0x3F,0x3F}
#define         MidnightBlue           {0x19,0x19,0x70}
#define         MinimumWhite           {0x1F,0x1F,0x1F}
#define         MintCream              {0xF5,0xFF,0xFA}
#define         MistyRose              {0xFF,0xE4,0xE1}
#define         Moccasin               {0xFF,0xE4,0xB5}
#define         NavajoWhite            {0xFF,0xDE,0xAD}
#define         Navy                   {0x00,0x00,0x80}
#define         OldLace                {0xFD,0xF5,0xE6}
#define         Olive                  {0x80,0x80,0x00}
#define         OliveDrab              {0x6B,0x8E,0x23}
#define         Orange                 {0xFF,0xA5,0x00}
#define         OrangeRed              {0xFF,0x45,0x00}
#define         Orchid                 {0xDA,0x70,0xD6}
#define         PaleGoldenrod          {0xEE,0xE8,0xAA}
#define         PaleGreen              {0x98,0xFB,0x98}
#define         PaleTurquoise          {0xAF,0xEE,0xEE}
#define         PaleVioletRed          {0xDB,0x70,0x93}
#define         PapayaWhip             {0xFF,0xEF,0xD5}
#define         PeachPuff              {0xFF,0xDA,0xB9}
#define         Peru                   {0xCD,0x85,0x3F}
#define         Pink                   {0xFF,0xC0,0xCB}
#define         Plaid                  {0xCC,0x55,0x33}
#define         Plum                   {0xDD,0xA0,0xDD}
#define         PowderBlue             {0xB0,0xE0,0xE6}
#define         Purple                 {0x80,0x00,0x80}
#define         Red                    {0xFF,0x00,0x00}
#define         RosyBrown              {0xBC,0x8F,0x8F}
#define         RoyalBlue              {0x41,0x69,0xE1}
#define         SaddleBrown            {0x8B,0x45,0x13}
#define         Salmon                 {0xFA,0x80,0x72}
#define         SandyBrown             {0xF4,0xA4,0x60}
#define         SeaGreen               {0x2E,0x8B,0x57}
#define         Seashell               {0xFF,0xF5,0xEE}
#define         Sienna                 {0xA0,0x52,0x2D}
#define         Silver                 {0xC0,0xC0,0xC0}
#define         SkyBlue                {0x87,0xCE,0xEB}
#define         SlateBlue              {0x6A,0x5A,0xCD}
#define         SlateGray              {0x70,0x80,0x90}
#define         SlateGrey              {0x70,0x80,0x90}
#define         Snow                   {0xFF,0xFA,0xFA}
#define         SpringGreen            {0x00,0xFF,0x7F}
#define         SteelBlue              {0x46,0x82,0xB4}
#define         Tan                    {0xD2,0xB4,0x8C}
#define         Teal                   {0x00,0x80,0x80}
#define         Thistle                {0xD8,0xBF,0xD8}
#define         Tomato                 {0xFF,0x63,0x47}
#define         Turquoise              {0x40,0xE0,0xD0}
#define         Violet                 {0xEE,0x82,0xEE}
#define         Wheat                  {0xF5,0xDE,0xB3}
#define         White                  {0xFF,0xFF,0xFF}
#define         WhiteSmoke             {0xF5,0xF5,0xF5}
#define         Yellow                 {0xFF,0xFF,0x00}
#define         YellowGreen            {0x9A,0xCD,0x32}
#define         FairyLight             {0xFF,0xE4,0x2D}
#define         FairyLightNCC          {0xFF,0x9D,0x2A}

#define         NHSBlue                {0x00,0x5E,0xB8}

// Predefined RGB colors
extern cRGB CRGBAliceBlue;
extern cRGB CRGBAmethyst;
extern cRGB CRGBAntiqueWhite;
extern cRGB CRGBAqua;
extern cRGB CRGBAquamarine;
extern cRGB CRGBAzure;
extern cRGB CRGBBeige;
extern cRGB CRGBBisque;
extern cRGB CRGBBlack;
extern cRGB CRGBBlanchedAlmond;
extern cRGB CRGBBlue;
extern cRGB CRGBBlueViolet;
extern cRGB CRGBBrown;
extern cRGB CRGBBurlyWood;
extern cRGB CRGBCadetBlue;
extern cRGB CRGBChartreuse;
extern cRGB CRGBChocolate;
extern cRGB CRGBCoral;
extern cRGB CRGBCornflowerBlue;
extern cRGB CRGBCornsilk;
extern cRGB CRGBCrimson;
extern cRGB CRGBCyan;
extern cRGB CRGBDarkBlue;
extern cRGB CRGBDarkCyan;
extern cRGB CRGBDarkGoldenrod;
extern cRGB CRGBDarkGray;
extern cRGB CRGBDarkGrey;
extern cRGB CRGBDarkGreen;
extern cRGB CRGBDarkKhaki;
extern cRGB CRGBDarkMagenta;
extern cRGB CRGBDarkOliveGreen;
extern cRGB CRGBDarkOrange;
extern cRGB CRGBDarkOrchid;
extern cRGB CRGBDarkRed;
extern cRGB CRGBDarkSalmon;
extern cRGB CRGBDarkSeaGreen;
extern cRGB CRGBDarkSlateBlue;
extern cRGB CRGBDarkSlateGray;
extern cRGB CRGBDarkSlateGrey;
extern cRGB CRGBDarkTurquoise;
extern cRGB CRGBDarkViolet;
extern cRGB CRGBDeepPink;
extern cRGB CRGBDeepSkyBlue;
extern cRGB CRGBDimGray;
extern cRGB CRGBDimGrey;
extern cRGB CRGBDodgerBlue;
extern cRGB CRGBFireBrick;
extern cRGB CRGBFloralWhite;
extern cRGB CRGBForestGreen;
extern cRGB CRGBFuchsia;
extern cRGB CRGBGainsboro;
extern cRGB CRGBGhostWhite;
extern cRGB CRGBGold;
extern cRGB CRGBGoldenrod;
extern cRGB CRGBGray;
extern cRGB CRGBGrey;
extern cRGB CRGBGreen;
extern cRGB CRGBGreenYellow;
extern cRGB CRGBHoneydew;
extern cRGB CRGBHotPink;
extern cRGB CRGBIndianRed;
extern cRGB CRGBIndigo;
extern cRGB CRGBIvory;
extern cRGB CRGBKhaki;
extern cRGB CRGBLavender;
extern cRGB CRGBLavenderBlush;
extern cRGB CRGBLawnGreen;
extern cRGB CRGBLemonChiffon;
extern cRGB CRGBLightBlue;
extern cRGB CRGBLightCoral;
extern cRGB CRGBLightCyan;
extern cRGB CRGBLightGoldenrodYellow;
extern cRGB CRGBLightGreen;
extern cRGB CRGBLightGrey;
extern cRGB CRGBLightPink;
extern cRGB CRGBLightSalmon;
extern cRGB CRGBLightSeaGreen;
extern cRGB CRGBLightSkyBlue;
extern cRGB CRGBLightSlateGray;
extern cRGB CRGBLightSlateGrey;
extern cRGB CRGBLightSteelBlue;
extern cRGB CRGBLightYellow;
extern cRGB CRGBLime;
extern cRGB CRGBLimeGreen;
extern cRGB CRGBLinen;
extern cRGB CRGBMagenta;
extern cRGB CRGBMaroon;
extern cRGB CRGBMediumAquamarine;
extern cRGB CRGBMediumBlue;
extern cRGB CRGBMediumOrchid;
extern cRGB CRGBMediumPurple;
extern cRGB CRGBMediumSeaGreen;
extern cRGB CRGBMediumSlateBlue;
extern cRGB CRGBMediumSpringGreen;
extern cRGB CRGBMediumTurquoise;
extern cRGB CRGBMediumVioletRed;
extern cRGB CRGBMediumWhite;
extern cRGB CRGBMidnightBlue;
extern cRGB CRGBMinimumWhite;
extern cRGB CRGBMintCream;
extern cRGB CRGBMistyRose;
extern cRGB CRGBMoccasin;
extern cRGB CRGBNavajoWhite;
extern cRGB CRGBNavy;
extern cRGB CRGBOldLace;
extern cRGB CRGBOlive;
extern cRGB CRGBOliveDrab;
extern cRGB CRGBOrange;
extern cRGB CRGBOrangeRed;
extern cRGB CRGBOrchid;
extern cRGB CRGBPaleGoldenrod;
extern cRGB CRGBPaleGreen;
extern cRGB CRGBPaleTurquoise;
extern cRGB CRGBPaleVioletRed;
extern cRGB CRGBPapayaWhip;
extern cRGB CRGBPeachPuff;
extern cRGB CRGBPeru;
extern cRGB CRGBPink;
extern cRGB CRGBPlaid;
extern cRGB CRGBPlum;
extern cRGB CRGBPowderBlue;
extern cRGB CRGBPurple;
extern cRGB CRGBRed;
extern cRGB CRGBRosyBrown;
extern cRGB CRGBRoyalBlue;
extern cRGB CRGBSaddleBrown;
extern cRGB CRGBSalmon;
extern cRGB CRGBSandyBrown;
extern cRGB CRGBSeaGreen;
extern cRGB CRGBSeashell;
extern cRGB CRGBSienna;
extern cRGB CRGBSilver;
extern cRGB CRGBSkyBlue;
extern cRGB CRGBSlateBlue;
extern cRGB CRGBSlateGray;
extern cRGB CRGBSlateGrey;
extern cRGB CRGBSnow;
extern cRGB CRGBSpringGreen;
extern cRGB CRGBSteelBlue;
extern cRGB CRGBTan;
extern cRGB CRGBTeal;
extern cRGB CRGBThistle;
extern cRGB CRGBTomato;
extern cRGB CRGBTurquoise;
extern cRGB CRGBViolet;
extern cRGB CRGBWheat;
extern cRGB CRGBWhite;
extern cRGB CRGBWhiteSmoke;
extern cRGB CRGBYellow;
extern cRGB CRGBYellowGreen;
extern cRGB CRGBFairyLight;
extern cRGB CRGBFairyLightNCC;

extern cRGB CRGBNHSBlue;

#endif
