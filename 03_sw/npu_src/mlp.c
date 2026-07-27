/* NPU에서 돌 3층 MLP: 4 -> 8 -> 8 -> 3 (ReLU), 정수 연산 */
#define DT ((volatile int*)0x10000)
#define IN   (DT+0)          /* 입력 4        0x00 */
#define W1   (DT+8)          /* 4x8=32        0x20 */
#define B1   (DT+40)         /* 8             0xA0 */
#define W2   (DT+48)         /* 8x8=64        0xC0 */
#define B2   (DT+112)        /* 8            0x1C0 */
#define W3   (DT+120)        /* 8x3=24       0x1E0 */
#define B3   (DT+144)        /* 3            0x240 */
#define OUT  (DT+148)        /* 3            0x250 */
#define ARGM (DT+151)        /* argmax       0x25C */
#define DONE (DT+152)        /* done         0x260 */

static int h1[8], h2[8];
static inline int relu(int v){ return v>0?v:0; }

void _start(void){
    for(int i=0;i<8;i++){
        int a=B1[i];
        for(int j=0;j<4;j++) a += W1[j*8+i]*IN[j];
        h1[i]=relu(a);
    }
    for(int i=0;i<8;i++){
        int a=B2[i];
        for(int j=0;j<8;j++) a += W2[j*8+i]*h1[j];
        h2[i]=relu(a);
    }
    int best=0, bv=-2147483647;
    for(int i=0;i<3;i++){
        int a=B3[i];
        for(int j=0;j<8;j++) a += W3[j*3+i]*h2[j];
        OUT[i]=a;
        if(a>bv){bv=a;best=i;}
    }
    ARGM[0]=best;
    DONE[0]=1;
    for(;;);
}
