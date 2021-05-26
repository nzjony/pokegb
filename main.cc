#include <cstdio>
#include <cstdint>
#include <cstdlib>

using u8=uint8_t; using s8=int8_t; using u16=uint16_t; using u64=uint64_t;
u8 op, u, fc, *rom0, *rom1, io[0x200], vram[0x2000], wram[0x4000], eram[0x8000],
    *eram1 = eram, R[] = {19, 0, 216, 0, 77, 1, 176, 1, 0, 1, 254, 255},
    &C = R[0], &B = R[1], &E = R[2], &D = R[3], &L = R[4], &H = R[5], &F = R[6],
    &A = R[7], *R8[] = {&B, &C, &D, &E, &H, &L, &F, &A}, &JOY = io[0x100],
    &IF = io[0x10f], &LCDC = io[0x140], &LY = io[0x144], &IE = io[0x1ff],
    IME = 0, halt = 0;
u16 U, &BC = (u16 &)R[0], &DE = (u16 &)R[2], &HL = (u16 &)R[4],
       &AF = (u16 &)R[6], &PC = (u16 &)R[8], &SP = (u16 &)R[10],
       *R16[] = {&BC, &DE, &HL, &SP}, *R162[] = {&BC, &DE, &HL, &AF},
       &DIV = (u16 &)io[0x103], dot = 0;
s8 s;
u64 bcy, cy = 0;

u8 r(u16 a) { cy+=4;
  switch (a >> 12) {
    default: case 0: case 1: case 2: case 3: return rom0[a];
    case 4: case 5: case 6: case 7: return rom1[a & 0x3fff];
    case 8: case 9: return vram[a & 0x1fff];
    case 10: case 11: return eram1[a & 0x1fff];
    case 15: if (a >= 0xfe00) return io[a & 0x1ff];
    case 12: case 13: case 14: return wram[a & 0x3fff];
  }
}
void w(u16 a, u8 x) { cy+=4;
  switch (a >> 12) {
    default: case 0: case 1: /*ignore*/ break;
    case 2: case 3: rom1 = rom0 + ((x & 63) << 14); break;
    case 4: case 5: if (x <= 3) eram1 = eram + (x << 13); break;
    case 6: case 7: fprintf(stderr, ">>> %04x<=%02x\n", a, x); abort();
    case 8: case 9: vram[a & 0x1fff] = x; break;
    case 10: case 11: eram1[a & 0x1fff] = x; break;
    case 15: if (a >= 0xfe00) { io[a & 0x1ff] = x; break; } // fallthrough
    case 12: case 13: case 14: wram[a & 0x3fff] = x; break;
  }
}
void FS(u8 M,int Z,int N,int H,int C) { F=(F&M)|(Z<<7)|(N<<6)|(H<<5)|(C<<4); }
u8 r8() { return r(PC++); }
u16 r16() { u8 l=r8(),h=r8(); return (h<<8)|l; }
void jr(bool b) { s=r8(); if(b) { PC+=s;cy+=4; } }
void jp(bool b) { u16 a=r16(); if(b) { PC=a;cy+=4; } }
u16 pop() { u8 l=r(SP++), h=r(SP++); return (h<<8)|l; }
void push(u16 x) { w(--SP,x>>8);w(--SP,x&255); cy+=4; }
void call(u16 x) { push(PC); PC=x; }
void dec(u8& r) { u=--r; FS(0x10,u==0,1,(u&15)==15,0); }
void inc(u8& r) { u=++r; FS(0x10,u==0,0,(u&15)==0,0); }
void _and(u8 r) { A&=r; FS(0,A==0,0,1,0); }
void _or(u8 x) { A|=x; FS(0,A==0,0,0,0); }
void _xor(u8 x) { A^=x; FS(0,A==0,0,0,0); }
void add(u8 x) { u=A+x; FS(0,u==0,0,(A&15)+(x&15)>15,A+x>255);A=u; }
void add(u16 x) { FS(0x80,0,0,(HL&0xfff)+(x&0xfff)>0xfff,HL+x>0xffff);HL+=x;cy+=4; }
void adc(u8 x) { fc=((F>>4)&1);u=A+x+fc; FS(0,u==0,0,(A&15)+(x&15)+fc>15,A+x+fc>255);A=u; }
void sub(u8 x) { u=A-x; FS(0,u==0,1,(A&15)-(x&15)<0,A-x<0);A=u; }
void sbc(u8 x) { fc=((F>>4)&1);u=A-x-fc; FS(0,u==0,1,(A&15)-(x&15)-fc<0,A-x-fc<0);A=u; }
void cp(u8 x) { FS(0,A==x,1,(A&15)-(x&15)<0,A-x<0); }
void bit(u8 r, u8 mask) { FS(0x10,(r&mask)==0,0,1,0); }
void sla(u8& r) { fc=(r>>7)&1;r<<=1; FS(0,r==0,0,0,fc); }
void rr(u8& r) { fc=r&1;r=(r>>1)|((F<<3)&0x80);FS(0,r==0,0,0,fc); }
void swap(u8& r) { r=(r<<4)|(r>>4); FS(0,r==0,0,0,0); }

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  FILE* f = fopen(argv[1], "rb");
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  rom1 = (rom0 = (u8*)malloc(len)) + 0x8000;
  fread(rom0, 1, len, f);
  fclose(f);
  LCDC=0x91;DIV=0xac00;dot=32;

  while (true) {
    bcy=cy;
#define OP4(x) case x+0:case x+16:case x+32:case x+48
#define OP7(x) case x+0:case x+1:case x+2:case x+3:case x+4:case x+5:case x+7
#define OP7X(x) case x+0:case x+8:case x+16:case x+24:case x+32:case x+40:case x+56
#define OP8X(x) case x+0:case x+8:case x+16:case x+24:case x+32:case x+40:case x+48:case x+56
    JOY=0xff; 
    if (IME&IF&IE) {
      u8 irqix=__builtin_ctz(IE&IF);IF&=~(1<<irqix);call(0x40+irqix*8);halt=IME=0;cy+=8;
    } else if (!halt) {
      op = r8();
      fprintf(stderr,
              "A:%02x F:%c%c%c%c BC:%04x DE:%04x HL:%04x SP:%04x [cy:%llu] "
              "ppu:%c LY:%u|%04x: %02x\n",
              A, F & 0x80 ? 'Z' : '-', F & 0x40 ? 'N' : '-',
              F & 0x20 ? 'H' : '-', F & 0x10 ? 'C' : '-', BC, DE, HL, SP,
              cy - 4, LCDC & 0x80 ? '+' : '-', LY, PC - 1, op);
      switch (op) {
      case 0: break;
      OP4(0x01): *R16[op>>4]=r16(); break;
      case 0x02: case 0x12: w(*R16[op>>4], A); break;
      OP4(0x03): (*R16[op>>4])++; cy+=4; break;
      OP7X(0x04): inc(*R8[op>>3]); break;
      OP7X(0x05): dec(*R8[op>>3]); break;
      OP7X(0x06): *R8[op>>3]=r8(); break;
      case 0x07: fc=(A>>7);A=(A<<1)|fc;FS(0,0,0,0,fc); break;
      OP4(0x09): add(*R16[op>>4]); break;
      case 0x0a: case 0x1a: A=r(*R16[op>>4]); break;
      OP4(0x0b): (*R16[op>>4])--; cy+=4; break;
      case 0x0f: fc=A&1;A=(fc<<7)|(A>>1);FS(0,0,0,0,fc); break;
      case 0x18: jr(true); break;
      case 0x20: jr(!(F&0x80)); break;
      case 0x22: w(HL++, A); break;
      case 0x28: jr(F&0x80); break;
      case 0x2a: A=r(HL++); break;
      case 0x2f: A=~A; FS(0x81,0,1,1,0); break;
      case 0x30: jr(!(F&0x10)); break;
      case 0x32: w(HL--, A); break;
      case 0x34: u=r(HL)+1;FS(0x10,u==0,0,(u&15)==0,0);w(HL,u); break;
      case 0x35: u=r(HL)-1;FS(0x10,u==0,1,(u&15)==15,0);w(HL,u); break;
      case 0x36: w(HL,r8()); break;
      case 0x37: FS(0x80,0,0,0,1); break;
      case 0x38: jr(F&0x10); break;
      case 0x3a: A=r(HL--); break;
      case 0x3f: FS(0x80,0,0,0,!(F&0x10)); break;
      OP7(0x40): B=*R8[op&7]; break;
      OP7X(0x46): *R8[(op-0x46)>>3]=r(HL); break;
      OP7(0x48): C=*R8[op&7]; break;
      OP7(0x50): D=*R8[op&7]; break;
      OP7(0x58): E=*R8[op&7]; break;
      OP7(0x60): H=*R8[op&7]; break;
      OP7(0x68): L=*R8[op&7]; break;
      OP7(0x70): w(HL,*R8[op&7]); break;
      case 0x76: halt=true; break;
      OP7(0x78): A=*R8[op&7]; break;
      OP7(0x80): add(*R8[op&7]); break;
      case 0x86: add(r(HL)); break;
      OP7(0x88): adc(*R8[op&7]); break;
      OP7(0x90): sub(*R8[op&7]); break;
      case 0x96: sub(r(HL)); break;
      OP7(0x98): sbc(*R8[op&7]); break;
      OP7(0xa0): _and(*R8[op&7]); break;
      case 0xa6: _and(r(HL)); break;
      OP7(0xa8): _xor(*R8[op&7]); break;
      OP7(0xb0): _or(*R8[op&7]); break;
      case 0xb6: _or(r(HL)); break;
      OP7(0xb8): cp(*R8[op&7]); break;
      case 0xbe: cp(r(HL)); break;
      case 0xc0: cy+=4; if (!(F&0x80)) goto RET; break;
      OP4(0xc1): *R162[(op-0xc1)>>4]=pop(); break;
      case 0xc2: jp(!(F&0x80)); break;
      case 0xc3: PC=r16(); cy+=4; break;
      OP4(0xc5): push(*R162[(op-0xc5)>>4]); break;
      case 0xc6: add(r8()); break;
      case 0xc8: cy+=4; if (F&0x80) goto RET; break;
 RET: case 0xc9: PC=pop(); cy+=4; break;
      case 0xca: jp(F&0x80); break;
      case 0xcb: switch((op=r8())) {
        case 0x0b: fc=E&1;E=(fc<<7)|(E>>1);FS(0,E==0,0,0,fc); break;
        case 0x0e: u=r(HL);fc=u&1;u=(fc<<7)|(u>>1);FS(0,u==0,0,0,fc);w(HL,u); break;
        case 0x12: fc=D>>7;D=(D<<1)|((F>>4)&1);FS(0,D==0,0,0,fc); break;
        OP7(0x18): rr(*R8[op&7]); break;
        OP7(0x20): sla(*R8[op&7]); break;
        case 0x2a: fc=D&1;D=(s8)D>>1; FS(0,D==0,0,0,fc); break;
        OP7(0x30): swap(*R8[op&7]); break;
        case 0x36: u=r(HL);u=(u<<4)|(u>>4); FS(0,u==0,0,0,0);w(HL,u); break;
        case 0x3f: fc=A&1;A>>=1; FS(0,A==0,0,0,fc); break;
        OP7(0x40): bit(*R8[op&7],1); break;
        OP7(0x48): bit(*R8[op&7],2); break;
        OP7(0x50): bit(*R8[op&7],4); break;
        OP7(0x58): bit(*R8[op&7],8); break;
        OP7(0x60): bit(*R8[op&7],0x10); break;
        OP7(0x68): bit(*R8[op&7],0x20); break;
        OP7(0x70): bit(*R8[op&7],0x40); break;
        OP7(0x78): bit(*R8[op&7],0x80); break;
        case 0x87: A&=~1; break;
        OP7(0x88): *R8[op&7]&=~2; break;
        OP7(0x90): *R8[op&7]&=~4; break;
        OP7(0xa8): *R8[op&7]&=~0x20; break;
        OP8X(0x46): bit(r(HL),1<<((op-0x46)>>3)); break; 
        OP8X(0x86): w(HL,r(HL)&~(1<<((op-0x86)>>3))); break; 
        OP8X(0xc6): w(HL,r(HL)|(1<<((op-0xc6)>>3))); break;
        OP7(0xc8): *R8[op&7]|=2; break;
        OP7(0xf8): *R8[op&7]|=0x80; break;
        default: goto BAD;
      } break;
      case 0xcc: U=r16(); if (F&0x80) call(U); break;
      case 0xcd: call(r16()); break;
      case 0xd0: cy+=4; if (!(F&0x10)) goto RET; break;
      case 0xd2: jp(!(F&0x10)); break;
      case 0xd6: sub(r8()); break;
      case 0xd8: cy+=4; if (F&0x10) goto RET; break;
      case 0xd9: IME=0x1f; goto RET;
      case 0xda: jp(F&0x10); break;
      case 0xe0: w(0xff00|r8(),A); break;
      case 0xe2: w(0xff00|C,A); break;
      case 0xe6: _and(r8()); break;
      case 0xe9: PC=HL; break;
      case 0xea: w(r16(),A); break;
      case 0xee: _xor(r8()); break;
      case 0xf0: A=r(0xff00|r8()); break;
      case 0xf3: IME=0; break;
      case 0xf6: _or(r8()); break;
      case 0xf8: u=r8();FS(0,0,0,(u8)SP+u>255,SP&15+u&15>15);HL=SP+(s8)u;cy+=4; break;
      case 0xf9: SP=HL;cy+=4; break;
      case 0xfa: A=r(r16()); break;
      case 0xfb: IME=0x1f; break;
      case 0xfe: cp(r8()); break;
 BAD: default:
        fprintf(stderr, "unknown opcode: %02x\n", op);
        abort();
        break;
    }
    } else { cy += 4; }
    DIV+=cy-bcy;
    for(;bcy<cy;++bcy) {
      if (LCDC&0x80) {
        ++dot;
        switch (LY) {
          case 144: if (dot==1) IF|=1; goto LINE;
    LINE: default: if(dot==456) { LY=(LY+1)%154; dot=0; }break;
        }
      } else {
        LY=dot=0;
      }
    }
  }
}
