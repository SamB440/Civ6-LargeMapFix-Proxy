// GENERATED from a disassembly of CivilizationVI_DX12.exe -- do not edit by hand.
// The generator is not part of this repository: it needs a copy of the game
// executable, which is why the table is checked in rather than built.
//
// The relocation table for widening the renderer's resource handle space
// from 32768 to 65534. Every entry rewrites one 4-byte field in .text.
//
// `oldv` is the value that MUST already be there. The installer checks all
// 320 before writing any, so a different build of the game fails cleanly.
#pragma once
#include <cstdint>

namespace Widen {

static const uint32_t OBJ_RVA   = 0x6DB3840;   // the resource list, in .data
static const uint32_t OLD_SPAN  = 0x6A1180;
static const uint32_t NEW_SPAN  = 0xD41180;
static const uint32_t GUARD_RVA = 0x6DB4000;   // first page wholly owned by the object
static const uint32_t GUARD_END = 0x7454000;   // last page wholly owned by it
// The two slivers that share a page with a neighbour and so cannot be
// protected. Dead after relocation, so they are poisoned and polled.
static const uint32_t POISON1_RVA = 0x6DB3840;  // header, below the first full page
static const uint32_t POISON1_LEN = 0x7C0;
static const uint32_t POISON2_RVA = 0x7454000;  // record-array tail, above the last
static const uint32_t POISON2_LEN = 0x9C0;

// rva: site in .text   len: instruction length   field: byte offset of the
// 4-byte value inside it   oldv/newv: before and after
struct Edit { uint32_t rva; uint8_t len; uint8_t field; int32_t oldv; int32_t newv; };

// Plain displacements: [reg + <disp32>] where reg holds the object.
static const Edit kDisp[] = {
    { 0x0940613,  6,  2, 0x000A1150, 0x00141150 },  // cmp dword ptr [rbx + 0xa1150], esi
    { 0x094061B,  6,  2, 0x000A1150, 0x00141150 },  // mov ecx, dword ptr [rbx + 0xa1150]
    { 0x0940621,  7,  3, 0x000A1140, 0x00141140 },  // mov rax, qword ptr [rbx + 0xa1140]
    { 0x094062E,  6,  2, 0x000A1150, 0x00141150 },  // dec dword ptr [rbx + 0xa1150]
    { 0x0941533,  6,  2, 0x000A1138, 0x00141138 },  // cmp dword ptr [rbx + 0xa1138], esi
    { 0x094153B,  6,  2, 0x000A1138, 0x00141138 },  // mov ecx, dword ptr [rbx + 0xa1138]
    { 0x0941541,  7,  3, 0x000A1128, 0x00141128 },  // mov rax, qword ptr [rbx + 0xa1128]
    { 0x094154E,  6,  2, 0x000A1138, 0x00141138 },  // dec dword ptr [rbx + 0xa1138]
    { 0x0944FBD,  7,  3, 0x000A0D00, 0x00140D00 },  // lea rdi, [rsi + 0xa0d00]
    { 0x094503D,  7,  3, 0x000A0D00, 0x00140D00 },  // lea rdi, [rsi + 0xa0d00]
    { 0x094875B,  8,  4, 0x00020D08, 0x00040D08 },  // mov dword ptr [r15 + rax*8 + 0x20d08], edx
    { 0x09487DD,  8,  4, 0x00020D08, 0x00040D08 },  // mov eax, dword ptr [r12 + rbx*8 + 0x20d08]
    { 0x0948803,  8,  4, 0x00020D08, 0x00040D08 },  // mov dword ptr [r12 + rbx*8 + 0x20d08], eax
    { 0x094880B,  8,  4, 0x00020D00, 0x00040D00 },  // mov rax, qword ptr [r12 + rbx*8 + 0x20d00]
    { 0x094882E,  9,  5, 0x000A1180, 0x00141180 },  // movups xmm0, xmmword ptr [rcx + r12 + 0xa1180]
    { 0x0948837,  8,  4, 0x000A11A4, 0x001411A4 },  // mov r8d, dword ptr [rcx + r12 + 0xa11a4]
    { 0x094883F,  9,  5, 0x000A1190, 0x00141190 },  // movups xmm1, xmmword ptr [rcx + r12 + 0xa1190]
    { 0x094A5B1,  8,  4, 0x000A11A8, 0x001411A8 },  // lea rdx, [r12 + 0xa11a8]
    { 0x094A5CE,  7,  3, 0x00020D00, 0x00040D00 },  // lea rcx, [rbx + 0x20d00]
    { 0x094A5DA,  7,  3, 0x00020D00, 0x00040D00 },  // mov rcx, qword ptr [rbx + 0x20d00]
    { 0x094A64D,  9,  5, 0x000A118A, 0x0014118A },  // movzx eax, word ptr [rdi + r12 + 0xa118a]
    { 0x094A656,  9,  5, 0x000A1188, 0x00141188 },  // movzx ecx, word ptr [rdi + r12 + 0xa1188]
    { 0x094A65F,  8,  4, 0x000A119C, 0x0014119C },  // mov r9d, dword ptr [rdi + r12 + 0xa119c]
    { 0x094A667,  8,  4, 0x000A11A0, 0x001411A0 },  // mov r8d, dword ptr [rdi + r12 + 0xa11a0]
    { 0x094A673,  8,  4, 0x000A1184, 0x00141184 },  // mov eax, dword ptr [rdi + r12 + 0xa1184]
    { 0x094DC81,  7,  3, 0x000A1128, 0x00141128 },  // lea rdi, [rbx + 0xa1128]
    { 0x094DCA6,  7,  3, 0x000A1140, 0x00141140 },  // lea rdi, [rbx + 0xa1140]
    { 0x095B9B3,  8,  4, 0x00020D00, 0x00040D00 },  // mov r13, qword ptr [rcx + r8*8 + 0x20d00]
    { 0x095B9C3,  8,  4, 0x00020D00, 0x00040D00 },  // mov rcx, qword ptr [rcx + rax*8 + 0x20d00]
    { 0x095BBF7,  8,  4, 0x00020D08, 0x00040D08 },  // mov dword ptr [r12 + rcx*8 + 0x20d08], eax
    { 0x095BC0B,  9,  4, 0x000A11A0, 0x001411A0 },  // cmp dword ptr [rbx + r12 + 0xa11a0], 5
    { 0x095BC1A,  8,  4, 0x000A1228, 0x00141228 },  // mov rdx, qword ptr [rbx + r12 + 0xa1228]
    { 0x095BC36,  8,  4, 0x000A1228, 0x00141228 },  // mov rax, qword ptr [rbx + r12 + 0xa1228]
    { 0x095BDD8,  9,  4, 0x000A1223, 0x00141223 },  // test byte ptr [rbx + r12 + 0xa1223], 1
    { 0x095BE0A,  8,  4, 0x000A11A8, 0x001411A8 },  // lea r8, [r12 + 0xa11a8]
    { 0x095CF34,  8,  4, 0x00020D00, 0x00040D00 },  // mov rdx, qword ptr [rcx + rax*8 + 0x20d00]
    { 0x095CF56,  7,  3, 0x000A1218, 0x00141218 },  // mov ebx, dword ptr [rax + rcx + 0xa1218]
    { 0x095E785,  7,  3, 0x000A11F8, 0x001411F8 },  // lea r9, [rcx + 0xa11f8]
    { 0x095E7A1,  8,  4, 0x00020D00, 0x00040D00 },  // mov rsi, qword ptr [rcx + rax*8 + 0x20d00]
    { 0x095E7B6,  8,  4, 0x00020D00, 0x00040D00 },  // mov rdi, qword ptr [rcx + rax*8 + 0x20d00]
    { 0x095E851,  7,  3, 0x000A1184, 0x00141184 },  // mov r9d, dword ptr [r13 + 0xa1184]
    { 0x095E85A,  8,  4, 0x000A1188, 0x00141188 },  // movzx eax, word ptr [r13 + 0xa1188]
    { 0x095FC8F,  8,  4, 0x00020D00, 0x00040D00 },  // mov rdx, qword ptr [r9 + rcx*8 + 0x20d00]
    { 0x095FCA3,  9,  4, 0x00020D0C, 0x00040D0C },  // test byte ptr [r9 + rcx*8 + 0x20d0c], 1
    { 0x09601B5,  8,  4, 0x00020D00, 0x00040D00 },  // mov rax, qword ptr [rsi + rax*8 + 0x20d00]
    { 0x096029C,  8,  4, 0x00020D00, 0x00040D00 },  // mov rdx, qword ptr [rsi + rdx*8 + 0x20d00]
    { 0x09602EB,  7,  3, 0x000A1218, 0x00141218 },  // mov eax, dword ptr [rcx + rsi + 0xa1218]
    { 0x096A2D5,  8,  4, 0x000A11A4, 0x001411A4 },  // mov dword ptr [r8 + rdx + 0xa11a4], ebx
    { 0x096A33D,  7,  3, 0x000A1180, 0x00141180 },  // lea rdi, [rdx + 0xa1180]
    { 0x096A3F0,  7,  3, 0x000A11A8, 0x001411A8 },  // lea r9, [r14 + 0xa11a8]
    { 0x096A458,  8,  4, 0x000A11A8, 0x001411A8 },  // movups xmmword ptr [r14 + 0xa11a8], xmm0
    { 0x096A460,  8,  4, 0x000A11B8, 0x001411B8 },  // movups xmmword ptr [r14 + 0xa11b8], xmm1
    { 0x096A468,  9,  5, 0x000A11C8, 0x001411C8 },  // movsd qword ptr [r14 + 0xa11c8], xmm4
    { 0x096A481,  7,  3, 0x000A11A8, 0x001411A8 },  // lea r8, [r14 + 0xa11a8]
    { 0x096AAEC,  7,  3, 0x000A11A8, 0x001411A8 },  // lea rbp, [rax + 0xa11a8]
    { 0x096AAF3,  7,  3, 0x000A1218, 0x00141218 },  // lea r12, [rax + 0xa1218]
    { 0x096AB0D,  8,  4, 0x000A11A4, 0x001411A4 },  // mov dword ptr [rcx + rax + 0xa11a4], r15d
    { 0x096AD03,  8,  3, 0x00020D0C, 0x00040D0C },  // and dword ptr [rdi + rcx*8 + 0x20d0c], 0xfffffffe
    { 0x096AD0B,  8,  4, 0x00020D00, 0x00040D00 },  // mov qword ptr [rdi + rcx*8 + 0x20d00], rax
    { 0x096AD19,  7,  3, 0x00020D08, 0x00040D08 },  // mov dword ptr [rdi + rcx*8 + 0x20d08], eax
    { 0x096AD50,  8,  3, 0x000A1180, 0x00141180 },  // mov byte ptr [rcx + rdi + 0xa1180], 0
    { 0x096AD5C,  7,  3, 0x000A119C, 0x0014119C },  // mov dword ptr [rcx + rdi + 0xa119c], eax
    { 0x096AD68,  8,  4, 0x000A118C, 0x0014118C },  // mov word ptr [rcx + rdi + 0xa118c], ax
    { 0x096AD75,  8,  4, 0x000A118A, 0x0014118A },  // mov word ptr [rcx + rdi + 0xa118a], ax
    { 0x096AD7D,  7,  3, 0x000A1198, 0x00141198 },  // mov dword ptr [rcx + rdi + 0xa1198], ebx
    { 0x096AD89,  7,  3, 0x000A1184, 0x00141184 },  // mov dword ptr [rcx + rdi + 0xa1184], eax
    { 0x096AD95,  8,  4, 0x000A1188, 0x00141188 },  // mov word ptr [rcx + rdi + 0xa1188], ax
    { 0x096ADA2,  7,  3, 0x000A1183, 0x00141183 },  // mov byte ptr [rcx + rdi + 0xa1183], al
    { 0x096ADAD,  7,  3, 0x000A1190, 0x00141190 },  // mov dword ptr [rcx + rdi + 0xa1190], eax
    { 0x096ADB4, 11,  3, 0x000A1194, 0x00141194 },  // mov dword ptr [rcx + rdi + 0xa1194], 0
    { 0x096ADBF, 11,  3, 0x000A11A0, 0x001411A0 },  // mov dword ptr [rcx + rdi + 0xa11a0], 5
    { 0x096ADCA, 10,  4, 0x000A1181, 0x00141181 },  // mov word ptr [rcx + rdi + 0xa1181], 1
    { 0x096B73F,  7,  3, 0x000A11A8, 0x001411A8 },  // lea rdi, [r11 + 0xa11a8]
    { 0x096B749,  7,  3, 0x000A1180, 0x00141180 },  // lea r10, [r11 + 0xa1180]
    { 0x096B764,  7,  3, 0x000A1218, 0x00141218 },  // lea rbp, [r11 + 0xa1218]
    { 0x096BAD1,  9,  4, 0x00020D0C, 0x00040D0C },  // or dword ptr [r11 + r9*8 + 0x20d0c], 1
    { 0x096BADA,  8,  4, 0x00020D00, 0x00040D00 },  // mov qword ptr [r11 + r9*8 + 0x20d00], rax
    { 0x096BAE8,  8,  4, 0x00020D08, 0x00040D08 },  // mov dword ptr [r11 + r9*8 + 0x20d08], eax
    { 0x096BD2B,  7,  3, 0x000A11A8, 0x001411A8 },  // lea rdi, [r11 + 0xa11a8]
    { 0x096BD36,  7,  3, 0x000A1218, 0x00141218 },  // lea rbp, [r11 + 0xa1218]
    { 0x096C087,  9,  4, 0x00020D0C, 0x00040D0C },  // or dword ptr [r11 + r8*8 + 0x20d0c], 1
    { 0x096C09B,  8,  4, 0x00020D00, 0x00040D00 },  // mov qword ptr [r11 + r8*8 + 0x20d00], rax
    { 0x096C0A9,  8,  4, 0x00020D08, 0x00040D08 },  // mov dword ptr [r11 + r8*8 + 0x20d08], eax
    { 0x096C840,  7,  3, 0x000A11A8, 0x001411A8 },  // lea r13, [r8 + 0xa11a8]
    { 0x096C859,  8,  4, 0x000A11A4, 0x001411A4 },  // mov dword ptr [rdx + r8 + 0xa11a4], edi
    { 0x096C8BE,  7,  3, 0x000A1180, 0x00141180 },  // lea r12, [r8 + 0xa1180]
    { 0x096DAC7,  7,  3, 0x000A1218, 0x00141218 },  // lea rcx, [r14 + 0xa1218]
    { 0x096DADD,  7,  3, 0x000A11A8, 0x001411A8 },  // lea rdx, [rsi + 0xa11a8]
    { 0x096DAE9,  7,  3, 0x000A11F8, 0x001411F8 },  // mov rcx, qword ptr [rsi + 0xa11f8]
    { 0x096DAF0,  7,  3, 0x000A1180, 0x00141180 },  // lea rdx, [r14 + 0xa1180]
    { 0x096DAF7,  8,  4, 0x000A1215, 0x00141215 },  // movzx r8d, byte ptr [rsi + 0xa1215]
    { 0x096DB50,  7,  3, 0x000A11F8, 0x001411F8 },  // mov rax, qword ptr [rsi + 0xa11f8]
    { 0x096E276,  8,  4, 0x000A11A0, 0x001411A0 },  // mov eax, dword ptr [rsi + r12 + 0xa11a0]
    { 0x096E28A,  9,  4, 0x000A1223, 0x00141223 },  // test byte ptr [rsi + r12 + 0xa1223], 1
    { 0x096E29E,  8,  4, 0x00020D00, 0x00040D00 },  // mov rcx, qword ptr [r12 + rax*8 + 0x20d00]
    { 0x096E3F1,  7,  3, 0x000A11A8, 0x001411A8 },  // lea rcx, [rbx + 0xa11a8]
    { 0x096E3FD,  8,  4, 0x000A1218, 0x00141218 },  // lea rcx, [r12 + 0xa1218]
    { 0x096E417,  7,  3, 0x000A11A8, 0x001411A8 },  // lea rcx, [rbx + 0xa11a8]
    { 0x096E423,  8,  4, 0x000A1180, 0x00141180 },  // lea rcx, [r12 + 0xa1180]
    { 0x096F35B,  6,  2, 0x00020080, 0x00040080 },  // mov ecx, dword ptr [rbx + 0x20080]
    { 0x096F361,  7,  3, 0x000A0D00, 0x00140D00 },  // lea rsi, [rbx + 0xa0d00]
    { 0x096F382,  8,  4, 0x000204A8, 0x000404A8 },  // mov ebp, dword ptr [r14 + rbx + 0x204a8]
    { 0x096F390,  8,  4, 0x00020498, 0x00040498 },  // mov rdi, qword ptr [r14 + rbx + 0x20498]
    { 0x096F3FE,  8,  4, 0x000204A8, 0x000404A8 },  // mov dword ptr [r14 + rbx + 0x204a8], r8d
    { 0x096F406,  6,  2, 0x00020080, 0x00040080 },  // mov ecx, dword ptr [rbx + 0x20080]
    { 0x096F418,  6,  2, 0x00020080, 0x00040080 },  // mov dword ptr [rbx + 0x20080], ecx
    { 0x09701AD,  6,  2, 0x000A1120, 0x00141120 },  // cmp dword ptr [rbx + 0xa1120], esi
    { 0x09701C0,  7,  3, 0x000A1110, 0x00141110 },  // mov rax, qword ptr [rbx + 0xa1110]
    { 0x09701C7,  7,  3, 0x000A1128, 0x00141128 },  // lea rdi, [rbx + 0xa1128]
    { 0x09701DB,  7,  3, 0x000A1140, 0x00141140 },  // lea rdi, [rbx + 0xa1140]
    { 0x0970219,  6,  2, 0x000A1120, 0x00141120 },  // cmp esi, dword ptr [rbx + 0xa1120]
    { 0x0970226, 10,  2, 0x000A1120, 0x00141120 },  // mov dword ptr [rbx + 0xa1120], 0
    { 0x0972FA7,  9,  5, 0x00020D00, 0x00040D00 },  // movups xmmword ptr [r10 + rax*8 + 0x20d00], xmm0
    { 0x0972FC4,  9,  5, 0x000A1180, 0x00141180 },  // movups xmmword ptr [rcx + r10 + 0xa1180], xmm0
    { 0x0972FD3,  9,  5, 0x000A1190, 0x00141190 },  // movups xmmword ptr [rcx + r10 + 0xa1190], xmm1
    { 0x0972FE1,  8,  4, 0x000A11A0, 0x001411A0 },  // mov dword ptr [rcx + r10 + 0xa11a0], eax
    { 0x0972FFF,  9,  5, 0x000A11A8, 0x001411A8 },  // movups xmmword ptr [rcx + r10 + 0xa11a8], xmm0
    { 0x097300E,  9,  5, 0x000A11B8, 0x001411B8 },  // movups xmmword ptr [rcx + r10 + 0xa11b8], xmm1
    { 0x097301D,  9,  5, 0x000A11C8, 0x001411C8 },  // movups xmmword ptr [rcx + r10 + 0xa11c8], xmm0
    { 0x097302C,  9,  5, 0x000A11D8, 0x001411D8 },  // movups xmmword ptr [rcx + r10 + 0xa11d8], xmm1
    { 0x097303B,  9,  5, 0x000A11E8, 0x001411E8 },  // movups xmmword ptr [rcx + r10 + 0xa11e8], xmm0
    { 0x097304D,  9,  5, 0x000A11F8, 0x001411F8 },  // movups xmmword ptr [rcx + r10 + 0xa11f8], xmm1
    { 0x097305F,  9,  5, 0x000A1208, 0x00141208 },  // movups xmmword ptr [rcx + r10 + 0xa1208], xmm0
    { 0x0973071,  9,  5, 0x000A1218, 0x00141218 },  // movups xmmword ptr [rcx + r10 + 0xa1218], xmm1
    { 0x0973083,  9,  5, 0x000A1228, 0x00141228 },  // movups xmmword ptr [rcx + r10 + 0xa1228], xmm0
    { 0x0974001,  7,  3, 0x000204A4, 0x000404A4 },  // lea rax, [rbx + 0x204a4]
    { 0x097402C,  7,  3, 0x000A1118, 0x00141118 },  // mov qword ptr [rbx + 0xa1118], rcx
    { 0x0974039,  7,  3, 0x000A1120, 0x00141120 },  // mov qword ptr [rbx + 0xa1120], rcx
    { 0x0974040,  7,  3, 0x000A1110, 0x00141110 },  // mov qword ptr [rbx + 0xa1110], rcx
    { 0x0974047,  6,  2, 0x000A1118, 0x00141118 },  // mov dword ptr [rbx + 0xa1118], ecx
    { 0x097404D,  7,  3, 0x000A1130, 0x00141130 },  // mov qword ptr [rbx + 0xa1130], rcx
    { 0x0974054,  7,  3, 0x000A1138, 0x00141138 },  // mov qword ptr [rbx + 0xa1138], rcx
    { 0x097405B,  7,  3, 0x000A1128, 0x00141128 },  // mov qword ptr [rbx + 0xa1128], rcx
    { 0x0974062,  6,  2, 0x000A1130, 0x00141130 },  // mov dword ptr [rbx + 0xa1130], ecx
    { 0x0974068,  7,  3, 0x000A1148, 0x00141148 },  // mov qword ptr [rbx + 0xa1148], rcx
    { 0x097406F,  7,  3, 0x000A1150, 0x00141150 },  // mov qword ptr [rbx + 0xa1150], rcx
    { 0x0974076,  7,  3, 0x000A1140, 0x00141140 },  // mov qword ptr [rbx + 0xa1140], rcx
    { 0x097407D,  6,  2, 0x000A1148, 0x00141148 },  // mov dword ptr [rbx + 0xa1148], ecx
    { 0x0974083,  6,  2, 0x00020080, 0x00040080 },  // mov dword ptr [rbx + 0x20080], ecx
    { 0x0974089,  7,  3, 0x00020D00, 0x00040D00 },  // lea rcx, [rbx + 0x20d00]
};

// rip-relative references. newv is an OFFSET into the object, not a
// displacement: the installer computes the disp once it knows the block.
static const Edit kRip[] = {
    { 0x0024084,  7,  3, 0x06D8F7B5, 0x00000000 },  // lea rcx, [rip + 0x6d8f7b5]
    { 0x0940837,  8,  4, 0x06473041, 0x00000040 },  // lock cmpxchg dword ptr [rip + 0x6473041], edx
    { 0x0940845,  8,  4, 0x06473033, 0x00000040 },  // lock cmpxchg dword ptr [rip + 0x6473033], edx
    { 0x0940852,  7,  3, 0x06472FE7, 0x00000000 },  // lea rcx, [rip + 0x6472fe7]
    { 0x094086B, 10,  2, 0x0647300B, 0x00000040 },  // mov dword ptr [rip + 0x647300b], 0
    { 0x0943E3D,  7,  3, 0x0646F9FC, 0x00000000 },  // lea rcx, [rip + 0x646f9fc]
    { 0x0943E5E,  7,  3, 0x0646F9DB, 0x00000000 },  // lea rcx, [rip + 0x646f9db]
    { 0x094475A,  7,  3, 0x0646F0DF, 0x00000000 },  // lea rcx, [rip + 0x646f0df]
    { 0x09448B7,  7,  3, 0x0646EF82, 0x00000000 },  // lea rcx, [rip + 0x646ef82]
    { 0x0944945,  7,  3, 0x0646EEF4, 0x00000000 },  // lea rcx, [rip + 0x646eef4]
    { 0x09449D1,  7,  3, 0x0646EE68, 0x00000000 },  // lea rcx, [rip + 0x646ee68]
    { 0x0944AEF,  7,  3, 0x0646ED4A, 0x00000000 },  // lea rcx, [rip + 0x646ed4a]
    { 0x0944C6F,  7,  3, 0x0646EBCA, 0x00000000 },  // lea rcx, [rip + 0x646ebca]
    { 0x0944D0A,  7,  3, 0x0646EB2F, 0x00000000 },  // lea rcx, [rip + 0x646eb2f]
    { 0x0944D67,  7,  3, 0x0646EAD2, 0x00000000 },  // lea rcx, [rip + 0x646ead2]
    { 0x0944DDD,  7,  3, 0x0646EA5C, 0x00000000 },  // lea rcx, [rip + 0x646ea5c]
    { 0x0944E85,  7,  3, 0x0646E9B4, 0x00000000 },  // lea rcx, [rip + 0x646e9b4]
    { 0x0944EA9,  7,  3, 0x0646E990, 0x00000000 },  // lea rcx, [rip + 0x646e990]
    { 0x0948572,  7,  3, 0x0648BFC7, 0x00040D00 },  // lea rcx, [rip + 0x648bfc7]
    { 0x094858A,  7,  3, 0x0650C42F, 0x00141180 },  // lea rax, [rip + 0x650c42f]
    { 0x094861C,  7,  3, 0x0650C39D, 0x00141180 },  // lea rax, [rip + 0x650c39d]
    { 0x0948626,  7,  3, 0x0646B213, 0x00000000 },  // lea r15, [rip + 0x646b213]
    { 0x09487CB,  7,  3, 0x0646B06E, 0x00000000 },  // lea r12, [rip + 0x646b06e]
    { 0x0948AB8,  7,  3, 0x0646AE01, 0x00000080 },  // lea rcx, [rip + 0x646ae01]
    { 0x0948ACD,  7,  3, 0x0650BEEC, 0x00141180 },  // lea rcx, [rip + 0x650beec]
    { 0x094A5A4,  7,  3, 0x06469295, 0x00000000 },  // lea r12, [rip + 0x6469295]
    { 0x094C89D,  7,  3, 0x06508144, 0x001411A8 },  // lea rdx, [rip + 0x6508144]
    { 0x094CE85,  7,  3, 0x06507B5C, 0x001411A8 },  // lea rdx, [rip + 0x6507b5c]
    { 0x094D0BF,  7,  3, 0x0650791E, 0x001411A4 },  // lea rax, [rip + 0x650791e]
    { 0x094D0FF,  7,  3, 0x065078DE, 0x001411A4 },  // lea rax, [rip + 0x65078de]
    { 0x094DDB9,  7,  3, 0x06465A80, 0x00000000 },  // lea rcx, [rip + 0x6465a80]
    { 0x0950C72,  7,  3, 0x06462BC7, 0x00000000 },  // lea rcx, [rip + 0x6462bc7]
    { 0x0952C27,  7,  3, 0x06481912, 0x00040D00 },  // lea r8, [rip + 0x6481912]
    { 0x0952C4C,  7,  3, 0x06501D95, 0x001411A8 },  // lea rdx, [rip + 0x6501d95]
    { 0x095B802,  7,  3, 0x06478D37, 0x00040D00 },  // lea rax, [rip + 0x6478d37]
    { 0x095B8F9,  7,  3, 0x06478C40, 0x00040D00 },  // lea rdx, [rip + 0x6478c40]
    { 0x095B985,  7,  3, 0x064F905C, 0x001411A8 },  // lea rcx, [rip + 0x64f905c]
    { 0x095B9AC,  7,  3, 0x06457E8D, 0x00000000 },  // lea rcx, [rip + 0x6457e8d]
    { 0x095BB94,  7,  3, 0x064F8E4D, 0x001411A8 },  // lea rax, [rip + 0x64f8e4d]
    { 0x095BBAB,  7,  3, 0x0647898E, 0x00040D00 },  // lea r13, [rip + 0x647898e]
    { 0x095BBD7,  7,  3, 0x06457C62, 0x00000000 },  // lea r12, [rip + 0x6457c62]
    { 0x095BD89,  7,  3, 0x064F8C30, 0x00141180 },  // lea rdx, [rip + 0x64f8c30]
    { 0x095CEF0,  7,  3, 0x064F7AC9, 0x00141180 },  // lea rax, [rip + 0x64f7ac9]
    { 0x095CF1E,  7,  3, 0x0645691B, 0x00000000 },  // lea rcx, [rip + 0x645691b]
    { 0x095D1F0,  7,  3, 0x064F77C9, 0x00141180 },  // lea rax, [rip + 0x64f77c9]
    { 0x095DADE,  7,  3, 0x06476A5B, 0x00040D00 },  // lea r15, [rip + 0x6476a5b]
    { 0x095DB74,  7,  3, 0x064769C5, 0x00040D00 },  // lea r8, [rip + 0x64769c5]
    { 0x095DBA3,  7,  3, 0x064F6E16, 0x00141180 },  // lea rax, [rip + 0x64f6e16]
    { 0x095E77E,  7,  3, 0x064550BB, 0x00000000 },  // lea rcx, [rip + 0x64550bb]
    { 0x095E7DD,  7,  3, 0x064F6254, 0x001411F8 },  // lea rax, [rip + 0x64f6254]
    { 0x095FC24,  7,  3, 0x06453C15, 0x00000000 },  // lea r9, [rip + 0x6453c15]
    { 0x095FC89,  6,  2, 0x06453BB1, 0x00000000 },  // cmp eax, dword ptr [rip + 0x6453bb1]
    { 0x095FCCA,  7,  3, 0x06453B6F, 0x00000000 },  // lea r9, [rip + 0x6453b6f]
    { 0x095FD36,  7,  3, 0x06474803, 0x00040D00 },  // lea r9, [rip + 0x6474803]
    { 0x095FD9A,  7,  3, 0x0647479F, 0x00040D00 },  // lea r9, [rip + 0x647479f]
    { 0x095FF8C,  7,  3, 0x064538AD, 0x00000000 },  // cmp r14d, dword ptr [rip + 0x64538ad]
    { 0x0960178,  7,  3, 0x064536C1, 0x00000000 },  // lea rsi, [rip + 0x64536c1]
    { 0x0960288,  7,  3, 0x064535B1, 0x00000000 },  // lea rsi, [rip + 0x64535b1]
    { 0x096A2AE,  7,  3, 0x0646A28B, 0x00040D00 },  // lea rcx, [rip + 0x646a28b]
    { 0x096A2BF,  7,  3, 0x0644957A, 0x00000000 },  // lea rdx, [rip + 0x644957a]
    { 0x096A3EA,  6,  2, 0x06449454, 0x00000004 },  // sub eax, dword ptr [rip + 0x6449454]
    { 0x096AAE0,  7,  3, 0x06448D59, 0x00000000 },  // lea rax, [rip + 0x6448d59]
    { 0x096ACD6,  7,  3, 0x06448B63, 0x00000000 },  // lea rdi, [rip + 0x6448b63]
    { 0x096AE55,  7,  3, 0x064696E4, 0x00040D00 },  // lea r8, [rip + 0x64696e4]
    { 0x096B365,  7,  3, 0x064691D4, 0x00040D00 },  // lea rdx, [rip + 0x64691d4]
    { 0x096B568,  6,  2, 0x064482D6, 0x00000004 },  // sub eax, dword ptr [rip + 0x64482d6]
    { 0x096B733,  7,  3, 0x06448106, 0x00000000 },  // lea r11, [rip + 0x6448106]
    { 0x096BA73,  7,  3, 0x06447DC6, 0x00000000 },  // lea r11, [rip + 0x6447dc6]
    { 0x096BD10,  7,  3, 0x06447B29, 0x00000000 },  // lea r11, [rip + 0x6447b29]
    { 0x096C023,  7,  3, 0x06447816, 0x00000000 },  // lea r11, [rip + 0x6447816]
    { 0x096C197,  7,  3, 0x064476A2, 0x00000000 },  // lea rdx, [rip + 0x64476a2]
    { 0x096C828,  7,  3, 0x06447011, 0x00000000 },  // lea r8, [rip + 0x6447011]
    { 0x096C836,  7,  3, 0x06467D03, 0x00040D00 },  // lea rcx, [rip + 0x6467d03]
    { 0x096C9B5,  6,  2, 0x06446E89, 0x00000004 },  // sub eax, dword ptr [rip + 0x6446e89]
    { 0x096CB5C,  7,  3, 0x064679DD, 0x00040D00 },  // lea rcx, [rip + 0x64679dd]
    { 0x096DAC0,  7,  3, 0x06445D79, 0x00000000 },  // lea r14, [rip + 0x6445d79]
    { 0x096E232,  7,  3, 0x064E6727, 0x00141120 },  // cmp dword ptr [rip + 0x64e6727], r15d
    { 0x096E239,  7,  3, 0x06445600, 0x00000000 },  // lea r13, [rip + 0x6445600]
    { 0x096E249,  7,  3, 0x064662F0, 0x00040D00 },  // lea r13, [rip + 0x64662f0]
    { 0x096E250,  7,  3, 0x064455E9, 0x00000000 },  // lea r12, [rip + 0x64455e9]
    { 0x096E260,  7,  3, 0x064E66E9, 0x00141110 },  // mov rax, qword ptr [rip + 0x64e66e9]
    { 0x096E453,  7,  3, 0x064E6506, 0x00141120 },  // cmp r14d, dword ptr [rip + 0x64e6506]
    { 0x096E467,  7,  3, 0x064453D2, 0x00000000 },  // lea r13, [rip + 0x64453d2]
    { 0x09700BC,  7,  3, 0x0644377D, 0x00000000 },  // lea rcx, [rip + 0x644377d]
    { 0x09716A1, 11,  3, 0x06442194, 0x00000000 },  // mov qword ptr [rip + 0x6442194], 1
    { 0x09716B2,  6,  2, 0x06462208, 0x00040080 },  // mov dword ptr [rip + 0x6462208], esi
    { 0x09716B8,  7,  3, 0x06462E81, 0x00040D00 },  // lea rcx, [rip + 0x6462e81]
    { 0x09716C6,  7,  3, 0x064421F3, 0x00000080 },  // lea rcx, [rip + 0x64421f3]
    { 0x09716D8,  7,  3, 0x06462609, 0x000404A8 },  // lea rax, [rip + 0x6462609]
    { 0x09716DF,  6,  2, 0x064E3293, 0x00141138 },  // mov dword ptr [rip + 0x64e3293], esi
    { 0x09716E8,  6,  2, 0x064E32A2, 0x00141150 },  // mov dword ptr [rip + 0x64e32a2], esi
    { 0x09716EE,  6,  2, 0x064E326C, 0x00141120 },  // mov dword ptr [rip + 0x64e326c], esi
    { 0x0972109,  7,  3, 0x06462430, 0x00040D00 },  // lea rcx, [rip + 0x6462430]
    { 0x097211A,  7,  3, 0x064E289F, 0x00141180 },  // lea rcx, [rip + 0x64e289f]
    { 0x097215C, 10,  2, 0x064416DA, 0x00000000 },  // mov dword ptr [rip + 0x64416da], 0xb
    { 0x0972893,  7,  3, 0x06440FA6, 0x00000000 },  // lea rcx, [rip + 0x6440fa6]
    { 0x0972BA7,  7,  3, 0x06440C92, 0x00000000 },  // lea rcx, [rip + 0x6440c92]
    { 0x0972BB6,  7,  3, 0x06461983, 0x00040D00 },  // lea rax, [rip + 0x6461983]
    { 0x0972BDE,  7,  3, 0x064E1DDB, 0x00141180 },  // lea rax, [rip + 0x64e1ddb]
    { 0x0972F84,  7,  3, 0x064408B5, 0x00000000 },  // lea r10, [rip + 0x64408b5]
    { 0x0F99E3A,  7,  3, 0x05EBAB3F, 0x00141140 },  // mov rcx, qword ptr [rip + 0x5ebab3f]
    { 0x0F99E46,  7,  3, 0x05EBAB1B, 0x00141128 },  // mov rcx, qword ptr [rip + 0x5ebab1b]
    { 0x0F99E52, 10,  2, 0x05EBAAFC, 0x00141118 },  // cmp dword ptr [rip + 0x5ebaafc], 0x200
    { 0x0F99E5E,  7,  3, 0x05EBAAEB, 0x00141110 },  // mov rcx, qword ptr [rip + 0x5ebaaeb]
    { 0x0F99E6F,  7,  3, 0x05E3AADA, 0x00041110 },  // lea rbx, [rip + 0x5e3aada]
};

// The four size/limit immediates that define the handle space itself.
static const Edit kImm[] = {
    { 0x0940642,  6,  2, 0x00008000, 0x0000FFFF },  // AddDynamicResource: gate cmp ecx,0x8000
    { 0x094064D,  5,  1, 0x00008000, 0x0000FFFF },  // AddDynamicResource: mov edi,0x8000 origin
    { 0x0941561,  6,  2, 0x00008000, 0x0000FFFF },  // AddStaticResource: gate cmp ecx,0x8000 -- the combined-count cap that was still 32768
    { 0x0951DC7,  8,  4, 0x00008000, 0x00010000 },  // heap 0x14f3ed6c0 RTV: NumDescriptors -- the heap CreateRenderTargetView faulted on
    { 0x0951E26,  8,  4, 0x00008000, 0x00010000 },  // heap 0x14f3ed660 CBV_SRV_UAV: NumDescriptors
    { 0x0951E4D,  8,  4, 0x00008000, 0x00010000 },  // heap 0x14f3ed680 CBV_SRV_UAV: NumDescriptors
    { 0x0951E9B,  8,  4, 0x00008000, 0x00010000 },  // heap 0x14f3ed640 CBV_SRV_UAV: NumDescriptors
    { 0x095FDF6,  5,  1, 0x00002058, 0x00004058 },  // 0x14095fde0: __chkstk frame size -- also sets the memset length via lea edx,[rax-0x58]
    { 0x095FE1F,  8,  4, 0x00002098, 0x00004098 },  // 0x14095fde0: rsp+0x2098 local/arg above the bitmap
    { 0x095FE2D,  8,  4, 0x00002098, 0x00004098 },  // 0x14095fde0: rsp+0x2098 local/arg above the bitmap
    { 0x095FE3C,  8,  4, 0x00002050, 0x00004050 },  // 0x14095fde0: rsp+0x2050 local/arg above the bitmap
    { 0x095FE48,  8,  4, 0x00002048, 0x00004048 },  // 0x14095fde0: rsp+0x2048 local/arg above the bitmap
    { 0x095FE57,  8,  4, 0x00002040, 0x00004040 },  // 0x14095fde0: rsp+0x2040 local/arg above the bitmap
    { 0x095FE66,  8,  4, 0x00002038, 0x00004038 },  // 0x14095fde0: rsp+0x2038 local/arg above the bitmap
    { 0x095FE80,  7,  3, 0x000020A0, 0x000040A0 },  // 0x14095fde0: rsp+0x20a0 local/arg above the bitmap
    { 0x095FE90,  7,  3, 0x000020A8, 0x000040A8 },  // 0x14095fde0: rsp+0x20a8 local/arg above the bitmap
    { 0x095FEE4,  8,  4, 0x00002088, 0x00004088 },  // 0x14095fde0: rsp+0x2088 local/arg above the bitmap
    { 0x095FF53,  8,  4, 0x00002090, 0x00004090 },  // 0x14095fde0: rsp+0x2090 local/arg above the bitmap
    { 0x0960026,  8,  4, 0x00002080, 0x00004080 },  // 0x14095fde0: rsp+0x2080 local/arg above the bitmap
    { 0x0960061,  8,  4, 0x00002090, 0x00004090 },  // 0x14095fde0: rsp+0x2090 local/arg above the bitmap
    { 0x09600A3,  8,  4, 0x00002090, 0x00004090 },  // 0x14095fde0: rsp+0x2090 local/arg above the bitmap
    { 0x09600AB,  8,  4, 0x00002080, 0x00004080 },  // 0x14095fde0: rsp+0x2080 local/arg above the bitmap
    { 0x09600BD,  8,  4, 0x00002090, 0x00004090 },  // 0x14095fde0: rsp+0x2090 local/arg above the bitmap
    { 0x09600D0,  8,  4, 0x00002088, 0x00004088 },  // 0x14095fde0: rsp+0x2088 local/arg above the bitmap
    { 0x09600EB,  8,  4, 0x00002098, 0x00004098 },  // 0x14095fde0: rsp+0x2098 local/arg above the bitmap
    { 0x0960110,  8,  4, 0x00002038, 0x00004038 },  // 0x14095fde0: rsp+0x2038 local/arg above the bitmap
    { 0x0960118,  8,  4, 0x00002040, 0x00004040 },  // 0x14095fde0: rsp+0x2040 local/arg above the bitmap
    { 0x0960120,  8,  4, 0x00002048, 0x00004048 },  // 0x14095fde0: rsp+0x2048 local/arg above the bitmap
    { 0x0960128,  8,  4, 0x00002050, 0x00004050 },  // 0x14095fde0: rsp+0x2050 local/arg above the bitmap
    { 0x0960130,  7,  3, 0x00002058, 0x00004058 },  // 0x14095fde0: epilogue add rsp
    { 0x096A3E1,  5,  1, 0x00008000, 0x0000FFFF },  // 0x14096a290: 0x8000 - dynamic, capacity remaining
    { 0x096B55F,  5,  1, 0x00008000, 0x0000FFFF },  // 0x14096b2e0: 0x8000 - dynamic, capacity remaining
    { 0x096C9AB,  5,  1, 0x00008000, 0x0000FFFF },  // 0x14096c800: 0x8000 - dynamic, capacity remaining
    { 0x09716AC,  6,  2, 0x00080000, 0x00100000 },  // reset 0x140971631: zero len, main array +0x20d00
    { 0x09716CD,  6,  2, 0x00020000, 0x00040000 },  // reset 0x140971631: zero len, refcounts +0x80
    { 0x0972104,  5,  1, 0x00080000, 0x00100000 },  // clear 0x140972100: zero len, main array +0x20d00
    { 0x0972115,  5,  1, 0x00600000, 0x00C00000 },  // clear 0x140972100: zero len, records +0xa1180 -- the one that overran and crashed startup
    { 0x0974033,  6,  2, 0x00080000, 0x00100000 },  // ctor 0x140973ff0: zero len, main array [rbx+0x20d00]
    { 0x09740A5,  6,  2, 0x00020000, 0x00040000 },  // ctor 0x140973ff0: zero len, refcounts [rbx+0x80]
};

// Image-relative: [module_base + index*scale + <RVA>]. MSVC hoists the
// module base into a register and addresses globals off it. newv is an
// OFFSET into the object; the installer computes the displacement as
// (block + offset) - module_base, which is why these cannot be a plain
// disp edit like kDisp.
static const Edit kImgRel[] = {
    { 0x094702C,  8,  4, 0x06E549DC, 0x0014119C },  // mov eax, dword ptr [rdx + r9 + 0x6e549dc]
    { 0x094705F,  8,  4, 0x06E549DC, 0x0014119C },  // mov eax, dword ptr [rcx + r9 + 0x6e549dc]
    { 0x095D234,  8,  4, 0x06DD4540, 0x00040D00 },  // mov rdx, qword ptr [r14 + rax*8 + 0x6dd4540]
    { 0x095D256,  8,  4, 0x06E54A58, 0x00141218 },  // movsxd rbx, dword ptr [rax + r14 + 0x6e54a58]
    { 0x095ED2D,  8,  4, 0x06DD4540, 0x00040D00 },  // mov rdx, qword ptr [r9 + rdx*8 + 0x6dd4540]
    { 0x095ED8C,  8,  4, 0x06E549C4, 0x00141184 },  // mov eax, dword ptr [rcx + r9 + 0x6e549c4]
    { 0x095ED99,  9,  5, 0x06E549C8, 0x00141188 },  // movzx eax, word ptr [rcx + r9 + 0x6e549c8]
    { 0x095EDA7,  9,  5, 0x06E549C1, 0x00141181 },  // movzx eax, byte ptr [rcx + r9 + 0x6e549c1]
    { 0x095EDB4,  9,  5, 0x06E549CC, 0x0014118C },  // movzx eax, word ptr [rcx + r9 + 0x6e549cc]
    { 0x095EF00,  8,  4, 0x06E54A58, 0x00141218 },  // mov eax, dword ptr [rcx + r9 + 0x6e54a58]
    { 0x095F293,  8,  4, 0x06DD4540, 0x00040D00 },  // mov rbx, qword ptr [rax + rbx + 0x6dd4540]
    { 0x095F385,  8,  4, 0x06DD4540, 0x00040D00 },  // mov rdx, qword ptr [rdi + rdx*8 + 0x6dd4540]
    { 0x095FF93,  8,  4, 0x06DD4540, 0x00040D00 },  // mov r10, qword ptr [rdx + rax*8 + 0x6dd4540]
    { 0x095FFAD,  8,  3, 0x06DD454C, 0x00040D0C },  // test byte ptr [rdx + rax*8 + 0x6dd454c], 1
    { 0x096AE2C,  7,  3, 0x06E549C0, 0x00141180 },  // lea r14, [r10 + 0x6e549c0]
    { 0x096AE33,  7,  3, 0x06E549E8, 0x001411A8 },  // lea rbx, [r10 + 0x6e549e8]
    { 0x096AE3D,  7,  3, 0x06E549C0, 0x00141180 },  // lea rdi, [r10 + 0x6e549c0]
    { 0x096AE49,  7,  3, 0x06E549E8, 0x001411A8 },  // lea r11, [r10 + 0x6e549e8]
    { 0x096AEA2, 12,  4, 0x06E549E4, 0x001411A4 },  // mov dword ptr [rcx + r10 + 0x6e549e4], 0
    { 0x096B37A,  7,  3, 0x06E549E8, 0x001411A8 },  // lea r13, [r9 + 0x6e549e8]
    { 0x096B39A,  8,  4, 0x06E549E4, 0x001411A4 },  // mov dword ptr [rdx + r9 + 0x6e549e4], r8d
    { 0x096B3FC,  7,  3, 0x06E549C0, 0x00141180 },  // lea r12, [r9 + 0x6e549c0]
    { 0x096C281,  7,  3, 0x06E549E8, 0x001411A8 },  // lea rsi, [r8 + 0x6e549e8]
    { 0x096C2A5,  7,  3, 0x06E54A58, 0x00141218 },  // lea r13, [r8 + 0x6e54a58]
    { 0x096C32F,  9,  4, 0x06DD454C, 0x00040D0C },  // or dword ptr [r13 + rdi*8 + 0x6dd454c], 1
    { 0x096C338,  8,  4, 0x06DD4540, 0x00040D00 },  // mov qword ptr [r13 + rdi*8 + 0x6dd4540], rax
    { 0x096C346,  8,  4, 0x06DD4548, 0x00040D08 },  // mov dword ptr [r13 + rdi*8 + 0x6dd4548], eax
    { 0x096C35B,  7,  3, 0x06E549C0, 0x00141180 },  // lea r10, [r13 + 0x6e549c0]
    { 0x096CB84,  7,  3, 0x06E549E8, 0x001411A8 },  // lea rbp, [r9 + 0x6e549e8]
    { 0x096CB91,  8,  4, 0x06E549E4, 0x001411A4 },  // mov dword ptr [rdx + r9 + 0x6e549e4], r13d
    { 0x096CBF5,  7,  3, 0x06E549C0, 0x00141180 },  // lea rbx, [r9 + 0x6e549c0]
};

// Unwind metadata in .xdata for the one function whose STACK FRAME grows
// (0x14095fde0, the 2-bits-per-handle bitmap). The frame size and the
// register-save slots are encoded there as 2-byte fields scaled by 8;
// without these the unwinder restores rsp and the non-volatiles from the
// old offsets and any exception unwinding through the function corrupts.
// These are NOT in .text, so they need their own VirtualProtect.
struct Unwind { uint32_t rva; uint16_t oldv; uint16_t newv; };

static const Unwind kUnwind[] = {
    { 0x11E979E, 0x040B, 0x080B },  // primary 0x11e9798: UWOP_ALLOC_LARGE 0x2058 -> 0x4058
    { 0x11E97AE, 0x0407, 0x0807 },  // chained 0x11e97a8: UWOP_SAVE_NONVOL r14 0x2038 -> 0x4038
    { 0x11E97B2, 0x0408, 0x0808 },  // chained 0x11e97a8: UWOP_SAVE_NONVOL rdi 0x2040 -> 0x4040
    { 0x11E97B6, 0x0409, 0x0809 },  // chained 0x11e97a8: UWOP_SAVE_NONVOL rsi 0x2048 -> 0x4048
    { 0x11E97BA, 0x040A, 0x080A },  // chained 0x11e97a8: UWOP_SAVE_NONVOL rbx 0x2050 -> 0x4050
};

static const int kNDisp = 140;
static const int kNRip  = 105;
static const int kNImm  = 39;
static const int kNImgRel = 31;
static const int kNUnwind = 5;

}  // namespace Widen
