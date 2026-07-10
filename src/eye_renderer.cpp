/**
 * @file    eye_renderer.cpp
 * @brief   RobotEyes v9.0 — OCP 解耦管线实现
 * @author  Rennick
 * @date    2026-07-10
 */
#include "eye_renderer.h"
#include "expressions.h"
#include <math.h>
static inline float ease_in(float t) {return t*t;}
static inline float ease_out(float t) {float i=1.0f-t; return 1.0f-i*i;}
static inline int16_t clamp_i16(int16_t v,int16_t lo,int16_t hi) {if(v<lo)return lo; if(v>hi)return hi; return v;}
#ifdef EYE_STYLE_A
static const EyeStyle_t g_style={56,44,20,10,16,0.55f,-6,-7,4,7,5,2,-2,3,1};
#endif
#ifdef EYE_STYLE_B
static const EyeStyle_t g_style={46,32,16,0,13,0.30f,-5,-4,3,5,-3,2,0,0,0};
#endif
#ifdef EYE_STYLE_C
static const EyeStyle_t g_style={40,36,12,0,12,0.30f,-3,-7,3,0,0,0,0,0,0};
#endif
typedef void(*PupilDrawFunc)(U8G2*,const EyeGeom_t*);
static void draw_pupil_normal(U8G2*d,const EyeGeom_t*g){if(g->pupil_r>0)d->drawDisc(g->pupil_cx,g->pupil_cy,g->pupil_r,U8G2_DRAW_ALL);}
static void draw_pupil_heart(U8G2*d,const EyeGeom_t*g){
  int16_t cx=g->pupil_cx,cy=g->pupil_cy,br=g->pupil_r; if(br<3){draw_pupil_normal(d,g);return;}
  uint32_t bt=millis()%900; float s=1.0f;
  if(bt<120){float t=(float)bt/120.0f;s=1.0f+0.35f*sinf(t*M_PI);}
  else if(bt<260){float t=(float)(bt-120)/140.0f;s=1.0f+0.20f*sinf(t*M_PI);}
  float sw=1.0f+(s-1.0f)*0.85f,sh=s;
  int16_t rw=(int16_t)((float)br*sw),rh=(int16_t)((float)br*sh);
  if(rw<3)rw=3;if(rh<3)rh=3;
  d->drawDisc(cx-rw/2,cy-rw/4,rw/2+1,U8G2_DRAW_ALL);
  d->drawDisc(cx+rw/2,cy-rw/4,rw/2+1,U8G2_DRAW_ALL);
  d->drawBox(cx-rw/2,cy-rw/4,rw,rw/2+1);
  d->drawTriangle(cx-rw,cy,cx+rw,cy,cx,cy+rh);
  d->drawTriangle(cx-rw+1,cy,cx+rw-1,cy,cx,cy+rh-1);
}
static void draw_pupil_slit(U8G2*d,const EyeGeom_t*g){int16_t sw=g->pupil_r/4;if(sw<1)sw=1;d->drawBox(g->pupil_cx-sw,g->pupil_cy-g->pupil_r-2,sw*2,g->pupil_r*2+4);}
static void draw_pupil_none(U8G2*,const EyeGeom_t*){}
static void draw_pupil_shock(U8G2*d,const EyeGeom_t*g){
  int16_t cx=g->pupil_cx,cy=g->pupil_cy,r=g->pupil_r;
  d->drawCircle(cx,cy,r,U8G2_DRAW_ALL);
  if(r>3){d->setDrawColor(1);d->drawCircle(cx,cy,r-2,U8G2_DRAW_ALL);d->setDrawColor(0);}
  if((millis()/40)%2==0){
    d->drawLine(cx-6,cy-6,cx-15,cy-15);d->drawLine(cx+6,cy-6,cx+15,cy-15);
    d->drawLine(cx-6,cy+6,cx-15,cy+15);d->drawLine(cx+6,cy+6,cx+15,cy+15);
    d->drawLine(cx-14,cy,cx-8,cy);d->drawLine(cx+8,cy,cx+14,cy);
  }else{
    d->drawLine(cx-4,cy-7,cx-10,cy-17);d->drawLine(cx+4,cy-7,cx+10,cy-17);
    d->drawLine(cx-4,cy+7,cx-10,cy+17);d->drawLine(cx+4,cy+7,cx+10,cy+17);
    d->drawLine(cx,cy-16,cx,cy-8);d->drawLine(cx,cy+8,cx,cy+16);
  }
}
static void draw_pupil_happy(U8G2*d,const EyeGeom_t*g){
  int16_t cx=g->pupil_cx,cy=g->pupil_cy,r=g->pupil_r;if(r<2){draw_pupil_normal(d,g);return;}
  int16_t uw=r+1;
  for(int16_t dy=-r;dy<=0;dy++){float norm=(float)(dy+r)/(float)r,curve=1.0f-norm*norm;int16_t dx=(int16_t)((float)uw*curve*0.55f);if(dx<1)dx=1;d->drawHLine(cx-dx,cy+dy,dx*2);}
  for(int16_t dy=1;dy<=r;dy++){float norm=(float)dy/(float)r,curve=1.0f-norm*norm;int16_t dx=(int16_t)((float)uw*curve*0.45f);if(dx<1)dx=1;d->drawHLine(cx-dx,cy+dy,dx*2);}
  {uint32_t star_t=millis()%2500;float alpha=0.0f;
    if(star_t<75)alpha=(float)star_t/75.0f;
    else if(star_t<150)alpha=1.0f-(float)(star_t-75)/75.0f;
    if(alpha>0.01f){d->setDrawColor(1);uint8_t ss=(uint8_t)(alpha*3.0f);if(ss<1)ss=1;
      for(uint8_t s=0;s<ss;s++){int16_t sx=cx-r+(rand()%(r*2)),sy=cy-r+(rand()%(r*2));
        int16_t sl=2+(rand()%3);d->drawHLine(sx-sl,sy,sl*2);d->drawVLine(sx,sy-sl,sl*2);}
      d->setDrawColor(0);}}
}

static void draw_pupil_star(U8G2*d,const EyeGeom_t*g){
  int16_t cx=g->pupil_cx,cy=g->pupil_cy,r=g->pupil_r;if(r<3){draw_pupil_normal(d,g);return;}
  int16_t cr=r/3;if(cr<2)cr=2;d->drawDisc(cx,cy,cr,U8G2_DRAW_ALL);
  int16_t sl=r+2;
  d->drawLine(cx,cy-cr,cx,cy-sl);d->drawLine(cx,cy+cr,cx,cy+sl);
  d->drawLine(cx-cr,cy,cx-sl,cy);d->drawLine(cx+cr,cy,cx+sl,cy);
  if((millis()/60)%2==0){int16_t dg=(int16_t)((float)sl*0.7f);
    d->drawLine(cx-cr,cy-cr,cx-dg,cy-dg);d->drawLine(cx+cr,cy-cr,cx+dg,cy-dg);
    d->drawLine(cx-cr,cy+cr,cx-dg,cy+dg);d->drawLine(cx+cr,cy+cr,cx+dg,cy+dg);}
}

static const PupilDrawFunc s_pupil_draw[PUPIL_COUNT]={
  [PUPIL_NORMAL]=draw_pupil_normal,[PUPIL_HEART]=draw_pupil_heart,
  [PUPIL_SLIT]=draw_pupil_slit,[PUPIL_NONE]=draw_pupil_none,
  [PUPIL_SHOCK]=draw_pupil_shock,[PUPIL_HAPPY]=draw_pupil_happy,
  [PUPIL_STAR]=draw_pupil_star,
};

const EyeStyle_t* eye_style_get(void){return &g_style;}

void eye_geom_compute(EyeGeom_t* gm,const EyeConfig_t* cfg,const EyeStyle_t* s,bool is_left){
  const int16_t hw=s->eye_w/2,hh=s->eye_h/2;
  gm->hw=hw;gm->hh=hh;gm->is_left=is_left;
  gm->eye_l=cfg->cx-hw;gm->eye_t=cfg->cy-hh;gm->eye_r=cfg->cx+hw;gm->eye_b=cfg->cy+hh;
  gm->pupil_r=(int16_t)((float)s->pupil_r*cfg->cur_pupil_scale);if(gm->pupil_r<1)gm->pupil_r=1;
  int16_t ppx=(int16_t)(cfg->cur_look_x*(float)s->look_max/127.0f),ppy=(int16_t)(cfg->cur_look_y*(float)s->look_max/127.0f);
  gm->pupil_cx=clamp_i16(cfg->cx+ppx,gm->eye_l+gm->pupil_r+1,gm->eye_r-gm->pupil_r-1);
  gm->pupil_cy=clamp_i16(cfg->cy+ppy,gm->eye_t+gm->pupil_r+1,gm->eye_b-gm->pupil_r-1);
  gm->pupil_type=cfg->cur_pupil_type;
  float sp=s->shine_parallax;
  int16_t sx=(int16_t)(cfg->cur_look_x*sp*(float)s->look_max/127.0f),sy=(int16_t)(cfg->cur_look_y*sp*(float)s->look_max/127.0f);
  if(s->s1_r>0){gm->s1_x=clamp_i16(gm->pupil_cx+s->s1_dx+sx,gm->eye_l+s->s1_r+1,gm->eye_r-s->s1_r-1);gm->s1_y=clamp_i16(gm->pupil_cy+s->s1_dy+sy,gm->eye_t+s->s1_r+1,gm->eye_b-s->s1_r-1);}else{gm->s1_x=0;gm->s1_y=0;}
  if(s->s2_r>0){gm->s2_x=clamp_i16(gm->pupil_cx+s->s2_dx+sx,gm->eye_l+s->s2_r+1,gm->eye_r-s->s2_r-1);gm->s2_y=clamp_i16(gm->pupil_cy+s->s2_dy+sy,gm->eye_t+s->s2_r+1,gm->eye_b-s->s2_r-1);}else{gm->s2_x=0;gm->s2_y=0;}
  if(s->s3_r>0){gm->s3_x=clamp_i16(gm->pupil_cx+s->s3_dx+sx,gm->eye_l+s->s3_r+1,gm->eye_r-s->s3_r-1);gm->s3_y=clamp_i16(gm->pupil_cy+s->s3_dy+sy,gm->eye_t+s->s3_r+1,gm->eye_b-s->s3_r-1);}else{gm->s3_x=0;gm->s3_y=0;}
  float lt,lb;
  if(cfg->lid>0.001f){lt=cfg->lid;lb=cfg->lid*0.5f;}else{lt=is_left?cfg->cur_lid_top_l:cfg->cur_lid_top_r;lb=cfg->cur_lid_bottom;}
  gm->lid_top_base_y=gm->eye_t+(int16_t)((float)(hh*2+4)*lt);
  gm->lid_slope_px=(int16_t)(cfg->cur_lid_slope*(float)hh);
  int16_t yi=gm->lid_top_base_y+gm->lid_slope_px,yo=gm->lid_top_base_y-gm->lid_slope_px;
  gm->lid_y_left=is_left?yo:yi;gm->lid_y_right=is_left?yi:yo;
  if(lb>0.001f)gm->lid_bottom_h=(int16_t)((float)(hh+2)*lb);else gm->lid_bottom_h=0;
}

void eye_draw_body(U8G2*d,const EyeGeom_t*gm){const EyeStyle_t*s=eye_style_get();d->drawRBox(gm->eye_l,gm->eye_t,gm->eye_r-gm->eye_l+1,gm->eye_b-gm->eye_t+1,s->eye_radius);}
void eye_draw_pupil(U8G2*d,const EyeGeom_t*gm){d->setDrawColor(0);if(gm->pupil_type<PUPIL_COUNT&&s_pupil_draw[gm->pupil_type])s_pupil_draw[gm->pupil_type](d,gm);}
void eye_draw_shine(U8G2*d,const EyeGeom_t*gm){
  const EyeStyle_t*s=eye_style_get();
  if(gm->pupil_type==PUPIL_HEART||gm->pupil_type==PUPIL_SHOCK||gm->pupil_type==PUPIL_HAPPY)return;
  d->setDrawColor(1);
  if(s->s1_r>0)d->drawDisc(gm->s1_x,gm->s1_y,s->s1_r,U8G2_DRAW_ALL);
  if(s->s2_r>0)d->drawDisc(gm->s2_x,gm->s2_y,s->s2_r,U8G2_DRAW_ALL);
  if(s->s3_r>0)d->drawDisc(gm->s3_x,gm->s3_y,s->s3_r,U8G2_DRAW_ALL);
}
void eye_draw_lid_mask(U8G2*d,const EyeGeom_t*gm){
  d->setDrawColor(0);
  if(gm->lid_y_left!=gm->eye_t||gm->lid_y_right!=gm->eye_t){int16_t top=gm->eye_t-20;if(top<0)top=0;
    d->drawTriangle(gm->eye_l-4,top,gm->eye_r+4,top,gm->eye_l-4,gm->lid_y_left);
    d->drawTriangle(gm->eye_r+4,top,gm->eye_l-4,gm->lid_y_left,gm->eye_r+4,gm->lid_y_right);}
  if(gm->lid_bottom_h>0)d->drawBox(gm->eye_l-2,gm->eye_b+2-gm->lid_bottom_h,gm->eye_r-gm->eye_l+5,gm->lid_bottom_h+2);
  d->setDrawColor(1);
}

static void eye_draw_tears(U8G2*d,const EyeGeom_t*gm){
  int16_t pcx=gm->pupil_cx,pcy=gm->pupil_cy,pr=gm->pupil_r;
  int16_t toy=gm->eye_b-gm->lid_bottom_h;if(toy<pcy+pr)toy=pcy+pr;
  d->setDrawColor(1);int16_t wy=toy-1;d->drawBox(pcx-pr+2,wy,pr*2-4,2);
  if(millis()%600>300)d->drawBox(pcx-pr/2-1,pcy+pr/3,pr+2,2);
  d->setDrawColor(0);const uint32_t cm=2200;
  uint32_t t1=millis()%cm;int16_t y1=toy+(int16_t)((float)t1*0.010f);
  int16_t tmy=gm->eye_b-3;if(y1>tmy)y1=tmy;if(y1<toy)y1=toy;
  if(y1<tmy){int16_t x1=gm->is_left?(gm->eye_l+10):(gm->eye_r-10);d->drawDisc(x1,y1,3,U8G2_DRAW_ALL);d->drawTriangle(x1-3,y1,x1+3,y1,x1,y1-5);}
  uint32_t t2=(millis()+800)%cm;int16_t y2=toy+(int16_t)((float)t2*0.008f);
  if(y2>tmy)y2=tmy;if(y2<toy)y2=toy;
  if(y2<tmy){int16_t x2=gm->is_left?(gm->eye_l+18):(gm->eye_r-18);d->drawDisc(x2,y2,2,U8G2_DRAW_ALL);d->drawTriangle(x2-2,y2,x2+2,y2,x2,y2-4);}
}

void eye_draw_sweat(U8G2*d,const EyeGeom_t*gm){
  int16_t soy=gm->eye_t+4,sx=gm->is_left?(gm->eye_l+6):(gm->eye_r-6);
  d->setDrawColor(0);const uint32_t cm=900;uint32_t t=millis()%cm;
  int16_t y=soy+(int16_t)((float)t*0.022f);int16_t smy=gm->eye_b-2;
  if(y>smy)y=smy;if(y<soy)y=soy;
  if(y<smy){d->drawDisc(sx,y-1,2,U8G2_DRAW_ALL);d->drawTriangle(sx-2,y,sx+2,y,sx,y+4);}
  if(t>800&&t<900){d->setDrawColor(1);d->drawPixel(sx-1,smy-1);d->drawPixel(sx+1,smy-1);d->setDrawColor(0);}
}

void eye_render(U8G2*d,EyeConfig_t*cfg,bool is_left){
  const EyeStyle_t*s=eye_style_get();EyeGeom_t gm;
  eye_geom_compute(&gm,cfg,s,is_left);
  d->setDrawColor(1);eye_draw_body(d,&gm);
  eye_draw_pupil(d,&gm);
  if(cfg->active_expr==3)eye_draw_tears(d,&gm);
  if(cfg->active_expr==6)eye_draw_sweat(d,&gm);
  eye_draw_shine(d,&gm);
  eye_draw_lid_mask(d,&gm);
}

void eye_config_init(EyeConfig_t*cfg,uint8_t cx,uint8_t cy){
  cfg->cx=cx;cfg->cy=cy;cfg->lid=0.0f;cfg->target_look_x=0;cfg->target_look_y=0;cfg->cur_look_x=0.0f;cfg->cur_look_y=0.0f;
  cfg->active_expr=255;cfg->target_lid_top=0.0f;cfg->target_lid_top_l=0.0f;cfg->target_lid_top_r=0.0f;cfg->target_lid_bottom=0.0f;cfg->target_lid_slope=0.0f;cfg->target_pupil_scale=1.0f;cfg->target_pupil_type=PUPIL_NORMAL;cfg->cur_pupil_type=PUPIL_NORMAL;
  cfg->cur_lid_top=0.0f;cfg->cur_lid_top_l=0.0f;cfg->cur_lid_top_r=0.0f;cfg->cur_lid_bottom=0.0f;cfg->cur_lid_slope=0.0f;cfg->cur_pupil_scale=1.0f;
  cfg->anim_peak_scale=0.0f;cfg->anim_start_ms=0;cfg->anim_duration_ms=0;
  cfg->sleepy_phase_ms=0;cfg->sleepy_lid=0.0f;
  cfg->brow_phase=0.0f;cfg->brow_angry_phase=0.0f;cfg->brow_burst_timer=0.0f;cfg->brow_anim_phase=0.0f;cfg->brow_offset_l=0;cfg->brow_offset_r=0;
  cfg->tear_phase_ms=0;cfg->tear_phase2_ms=0;
  cfg->attention_next_ms=millis()+3000;cfg->attention_target_x=0;cfg->attention_target_y=0;cfg->attention_prev_x=0;cfg->attention_prev_y=0;cfg->attention_phase=0;
  cfg->overdrive_decay=0.0f;cfg->overdrive_amount=0.0f;
  cfg->idle_micro_next_ms=millis()+2000;cfg->idle_micro_type=0;cfg->idle_micro_lid_delta=0.0f;cfg->idle_micro_pupil_delta=0.0f;
  cfg->happy_wink_next_ms=millis()+3500;cfg->happy_wink_eye=0;cfg->happy_wink_start_ms=0;
  cfg->panic_scan_next_ms=millis()+120;
}

void eye_set_look(EyeConfig_t*cfg,int8_t x,int8_t y){cfg->target_look_x=x;cfg->target_look_y=y;}
void eye_look_update(EyeConfig_t*cfg){cfg->cur_look_x+=((float)cfg->target_look_x-cfg->cur_look_x)*LOOK_SMOOTH_FACTOR;cfg->cur_look_y+=((float)cfg->target_look_y-cfg->cur_look_y)*LOOK_SMOOTH_FACTOR;}
void eye_look_reset(EyeConfig_t*cfg){cfg->target_look_x=0;cfg->target_look_y=0;}
void eye_set_expression(EyeConfig_t*cfg,uint8_t ei){
  if(ei>=8)return;const ExpressionDef_t*e=&EXPRESSIONS[ei];cfg->active_expr=ei;
  cfg->target_lid_top=e->lid_top;cfg->target_lid_top_l=e->lid_top_l;cfg->target_lid_top_r=e->lid_top_r;cfg->target_lid_bottom=e->lid_bottom;cfg->target_lid_slope=e->lid_slope;cfg->target_pupil_type=e->pupil_type;
  cfg->sleepy_phase_ms=0;cfg->sleepy_lid=e->lid_top;
  cfg->brow_phase=0.0f;cfg->brow_angry_phase=0.0f;cfg->brow_burst_timer=0.0f;cfg->brow_anim_phase=0.0f;cfg->brow_offset_l=0;cfg->brow_offset_r=0;
  cfg->tear_phase_ms=0;cfg->tear_phase2_ms=800;
  cfg->happy_wink_next_ms=millis()+3500;cfg->happy_wink_eye=0;cfg->happy_wink_start_ms=0;
  cfg->panic_scan_next_ms=millis()+120;
  if(ei==7){cfg->target_pupil_scale=e->pupil_scale;cfg->anim_peak_scale=0.0f;cfg->anim_start_ms=0;cfg->anim_duration_ms=0;}
  else if(e->anim_peak>0.001f||e->anim_peak<-0.001f){cfg->target_pupil_scale=e->anim_peak;cfg->anim_peak_scale=e->anim_peak;cfg->anim_start_ms=millis();cfg->anim_duration_ms=e->anim_ms;}
  else{cfg->target_pupil_scale=e->pupil_scale;cfg->anim_peak_scale=0.0f;cfg->anim_start_ms=0;cfg->anim_duration_ms=0;}
  if(e->brow_anim==BROW_ANIM_RAISE_BOUNCE){cfg->overdrive_amount=e->brow_amp*1.8f;cfg->overdrive_decay=0.85f;}
  else{cfg->overdrive_amount=0.0f;cfg->overdrive_decay=0.0f;}
}

void eye_expr_update(EyeConfig_t*cfg,uint32_t now_ms){
  if(cfg->anim_peak_scale>0.001f||cfg->anim_peak_scale<-0.001f){uint32_t el=now_ms-cfg->anim_start_ms;if(el>=cfg->anim_duration_ms){cfg->anim_peak_scale=0.0f;if(cfg->active_expr<8)cfg->target_pupil_scale=EXPRESSIONS[cfg->active_expr].pupil_scale;}}
  if(cfg->active_expr==1){
    if(cfg->happy_wink_eye==0){if(now_ms>=cfg->happy_wink_next_ms){uint32_t es=now_ms-cfg->happy_wink_next_ms;cfg->happy_wink_eye=(es%2==0)?1:2;cfg->happy_wink_start_ms=now_ms;}}
    else{uint32_t we=now_ms-cfg->happy_wink_start_ms;
      if(we<90){float t=(float)we/90.0f;if(cfg->happy_wink_eye==1)cfg->target_lid_top_l=ease_in(t);else cfg->target_lid_top_r=ease_in(t);}
      else if(we<180){float t=(float)(we-90)/90.0f;if(cfg->happy_wink_eye==1)cfg->target_lid_top_l=1.0f-ease_out(t);else cfg->target_lid_top_r=1.0f-ease_out(t);}
      else{if(cfg->happy_wink_eye==1)cfg->target_lid_top_l=EXPRESSIONS[1].lid_top_l;else cfg->target_lid_top_r=EXPRESSIONS[1].lid_top_r;cfg->happy_wink_eye=0;cfg->happy_wink_next_ms=now_ms+2500+(rand()%3000);}}
  }
  if(cfg->active_expr==4){
    uint32_t st=now_ms%600;
    if(st<150){cfg->target_lid_top_l=-0.05f;cfg->target_lid_top_r=0.35f;}
    else if(st<300){cfg->target_lid_top_l=0.35f;cfg->target_lid_top_r=-0.05f;}
    else if(st<450){cfg->target_lid_top_l=-0.08f;cfg->target_lid_top_r=-0.08f;}
    else{cfg->target_lid_top_l=0.10f;cfg->target_lid_top_r=0.10f;}
  }
  if(cfg->active_expr==7){cfg->target_pupil_scale=1.0f;}
  if(cfg->active_expr==5){
    cfg->sleepy_phase_ms+=33;uint32_t cy=cfg->sleepy_phase_ms%4000;
    if(cy<2200){float t=(float)cy/2200.0f;cfg->sleepy_lid=0.60f+t*0.35f;float dr=sin((float)cy*0.002f)*15.0f;cfg->target_look_x=(int8_t)dr;cfg->target_look_y=(int8_t)(cos((float)cy*0.003f)*8.0f);float bt=(float)cy/2200.0f;int8_t bs=(int8_t)(bt*bt*12.0f);cfg->brow_offset_l=-bs;cfg->brow_offset_r=-bs;}
    else if(cy<2500){float t=(float)(cy-2200)/300.0f;float sn=1.0f-t;cfg->sleepy_lid=0.95f-sn*0.75f;cfg->target_look_x=0;cfg->target_look_y=0;int8_t bp=(int8_t)(15.0f*expf(-t*4.0f));cfg->brow_offset_l=bp;cfg->brow_offset_r=bp;}
    else if(cy<3000){float t=(float)(cy-2500)/500.0f;float wb=sin(t*M_PI*2.0f)*0.08f;cfg->sleepy_lid=0.20f+wb;cfg->target_look_x=(int8_t)(sin(t*M_PI)*20.0f);cfg->target_look_y=(int8_t)(cos(t*M_PI*0.7f)*6.0f);cfg->brow_offset_l=(int8_t)(3.0f*(1.0f-t));cfg->brow_offset_r=(int8_t)(3.0f*(1.0f-t));}
    else{float t=(float)(cy-3000)/1000.0f;cfg->sleepy_lid=0.20f+t*0.40f;cfg->target_look_x=0;cfg->target_look_y=0;cfg->brow_offset_l=(int8_t)(-t*6.0f);cfg->brow_offset_r=(int8_t)(-t*6.0f);}
    cfg->target_lid_top=cfg->sleepy_lid;
  }
  if(cfg->active_expr==6){
    float br=0.80f+sinf((float)now_ms*0.018f)*0.15f;cfg->target_pupil_scale=br;
    if(now_ms>=cfg->panic_scan_next_ms){cfg->target_look_x=(int8_t)((rand()%81)-40);cfg->target_look_y=(int8_t)((rand()%41)-20);cfg->panic_scan_next_ms=now_ms+90+(rand()%121);}
  }
  if(cfg->active_expr<8&&cfg->active_expr!=5){
    const ExpressionDef_t*e=&EXPRESSIONS[cfg->active_expr];float f=e->brow_freq,a=e->brow_amp,as=e->brow_asymmetry,ol=0.0f,orr=0.0f;
    switch(e->brow_anim){
    case BROW_ANIM_BREATHE:{cfg->brow_anim_phase+=f;ol=sin(cfg->brow_anim_phase)*a;orr=ol;break;}
    case BROW_ANIM_TREMBLE:{cfg->brow_anim_phase+=f;float ca=sin(cfg->brow_anim_phase)*a,bu=0.0f;uint32_t cyc=millis()%(uint32_t)e->brow_burst_intv,bw=(uint32_t)e->brow_burst_intv/8;if(cyc<bw){float bt=(float)cyc/(float)bw;bu=sin(bt*M_PI)*e->brow_burst_amp;}ol=ca+bu;orr=(ca+bu)*as;break;}
    case BROW_ANIM_SOB:{cfg->brow_anim_phase+=f;ol=sin(cfg->brow_anim_phase)*a;orr=sin(cfg->brow_anim_phase+M_PI*as)*a;break;}
    case BROW_ANIM_RAISE_BOUNCE:{cfg->brow_anim_phase+=f;float sw=sinf(cfg->brow_anim_phase)*a*0.25f,ov=cfg->overdrive_amount;if(cfg->overdrive_decay>0.0f){cfg->overdrive_amount*=cfg->overdrive_decay;if(fabsf(cfg->overdrive_amount)<0.15f)cfg->overdrive_amount=0.0f;}ol=sw+ov;orr=ol;break;}
    case BROW_ANIM_SAG_DRIFT:{cfg->brow_anim_phase+=f;float d1=sinf(cfg->brow_anim_phase)*a*0.5f,d2=sinf(cfg->brow_anim_phase*0.3f+1.5f)*a*0.5f*as;ol=-a+d1;orr=-a+d2;break;}
    case BROW_ANIM_TWITCH:{uint32_t tc=(uint32_t)(cfg->brow_anim_phase*1000.0f),iv=1200+(tc*7%800),ps=tc%iv;if(ps<40){float t=(float)ps/40.0f;ol=a*t;orr=0.0f;}else if(ps<80){float t=(float)(ps-40)/40.0f;ol=a*(1.0f-t);orr=0.0f;}else{ol=0.0f;orr=0.0f;}cfg->brow_anim_phase+=f;break;}
    case BROW_ANIM_SWAY:{cfg->brow_anim_phase+=f;float dl=sinf(cfg->brow_anim_phase)*a;ol=dl;orr=-dl;break;}
    case BROW_ANIM_PANIC:{cfg->brow_anim_phase+=f;float tr=sinf(cfg->brow_anim_phase)*a,no=((float)(rand()%100)/100.0f-0.5f)*a*0.6f,dl=a+tr+no;ol=dl;orr=dl;break;}
    case BROW_ANIM_NONE:default:cfg->brow_anim_phase+=f;ol=0.0f;orr=0.0f;break;
    }
    cfg->brow_offset_l=(int8_t)ol;cfg->brow_offset_r=(int8_t)orr;
  }
  cfg->cur_lid_top+=(cfg->target_lid_top-cfg->cur_lid_top)*0.18f;cfg->cur_lid_top_l+=(cfg->target_lid_top_l-cfg->cur_lid_top_l)*0.18f;cfg->cur_lid_top_r+=(cfg->target_lid_top_r-cfg->cur_lid_top_r)*0.18f;cfg->cur_lid_bottom+=(cfg->target_lid_bottom-cfg->cur_lid_bottom)*0.18f;cfg->cur_lid_slope+=(cfg->target_lid_slope-cfg->cur_lid_slope)*0.18f;cfg->cur_pupil_scale+=(cfg->target_pupil_scale-cfg->cur_pupil_scale)*0.18f;cfg->cur_pupil_type=cfg->target_pupil_type;
}
void eye_attention_update(EyeConfig_t*cfg,uint32_t now_ms){
  if(cfg->active_expr!=0&&cfg->active_expr!=255)return;if(cfg->lid>0.1f)return;
  if(cfg->attention_phase==0){if(now_ms>=cfg->attention_next_ms){cfg->attention_prev_x=cfg->target_look_x;cfg->attention_prev_y=cfg->target_look_y;cfg->attention_target_x=(int8_t)((rand()%61)-30);cfg->attention_target_y=(int8_t)((rand()%41)-20);cfg->attention_phase=1;}}
  else if(cfg->attention_phase==1){float t=0.05f;float dx=(float)(cfg->attention_target_x-cfg->target_look_x)*t,dy=(float)(cfg->attention_target_y-cfg->target_look_y)*t;cfg->target_look_x+=(int8_t)dx;cfg->target_look_y+=(int8_t)dy;if(abs(cfg->attention_target_x-cfg->target_look_x)<=1&&abs(cfg->attention_target_y-cfg->target_look_y)<=1){cfg->attention_phase=2;cfg->attention_next_ms=now_ms+800+(rand()%1500);}}
  else if(cfg->attention_phase==2){if(now_ms>=cfg->attention_next_ms)cfg->attention_phase=3;}
  else{float t=0.03f;cfg->target_look_x+=(int8_t)((float)(-cfg->target_look_x)*t);cfg->target_look_y+=(int8_t)((float)(-cfg->target_look_y)*t);if(abs(cfg->target_look_x)<=1&&abs(cfg->target_look_y)<=1){cfg->target_look_x=0;cfg->target_look_y=0;cfg->attention_phase=0;cfg->attention_next_ms=now_ms+2000+(rand()%4000);}}
}

void eye_idle_micro_update(EyeConfig_t*cfg,uint32_t now_ms){
  if(cfg->lid>0.2f)return;
  if(cfg->idle_micro_type==0){if(now_ms>=cfg->idle_micro_next_ms){uint8_t r=rand()%6;if(r==0){cfg->idle_micro_type=1;cfg->idle_micro_pupil_delta=(rand()%2)?0.05f:-0.05f;}else if(r==1){cfg->idle_micro_type=2;}else if(r==2){cfg->idle_micro_type=3;cfg->idle_micro_lid_delta=0.04f;}else{cfg->idle_micro_next_ms=now_ms+1500+(rand()%3000);}}}
  else{uint32_t el=now_ms-(cfg->idle_micro_next_ms-500);if(el>120){if(cfg->idle_micro_type==1){cfg->target_pupil_scale-=cfg->idle_micro_pupil_delta;cfg->idle_micro_pupil_delta=0.0f;}else if(cfg->idle_micro_type==3){cfg->target_lid_top-=cfg->idle_micro_lid_delta;cfg->idle_micro_lid_delta=0.0f;}cfg->idle_micro_type=0;cfg->idle_micro_next_ms=now_ms+2000+(rand()%4000);}else{if(cfg->idle_micro_type==1)cfg->target_pupil_scale+=cfg->idle_micro_pupil_delta*0.05f;else if(cfg->idle_micro_type==2&&el<60)cfg->brow_offset_l=(int8_t)((rand()%5)-2);else if(cfg->idle_micro_type==3)cfg->target_lid_top+=cfg->idle_micro_lid_delta*0.1f;}}
}

void blink_state_init(BlinkState_t*st){st->phase=BLINK_IDLE;st->phase_start_ms=0;st->phase_duration_ms=0;st->next_blink_ms=millis()+BLINK_INTERVAL_MIN+(rand()%(BLINK_INTERVAL_MAX-BLINK_INTERVAL_MIN));}

void blink_state_update(BlinkState_t*st,EyeConfig_t*cfg,uint32_t now_ms){
  switch(st->phase){
  case BLINK_IDLE:if(now_ms>=st->next_blink_ms){st->phase=BLINK_CLOSING;st->phase_start_ms=now_ms;st->phase_duration_ms=BLINK_CLOSING_MS;}break;
  case BLINK_CLOSING:{float t=(float)(now_ms-st->phase_start_ms)/st->phase_duration_ms;if(t>=1.0f){cfg->lid=1.0f;st->phase=BLINK_HOLD;st->phase_start_ms=now_ms;st->phase_duration_ms=BLINK_HOLD_MS;}else cfg->lid=ease_in(t);break;}
  case BLINK_HOLD:if(now_ms-st->phase_start_ms>=st->phase_duration_ms){st->phase=BLINK_OPENING;st->phase_start_ms=now_ms;st->phase_duration_ms=BLINK_OPENING_MS;}break;
  case BLINK_OPENING:{float t=(float)(now_ms-st->phase_start_ms)/st->phase_duration_ms;if(t>=1.0f){cfg->lid=0.0f;st->phase=BLINK_IDLE;st->next_blink_ms=now_ms+BLINK_INTERVAL_MIN+(rand()%(BLINK_INTERVAL_MAX-BLINK_INTERVAL_MIN));}else cfg->lid=1.0f-ease_out(t);break;}
  }
}