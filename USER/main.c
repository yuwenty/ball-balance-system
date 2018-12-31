#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "lcd.h"
#include "usmart.h"  
#include "usart2.h"  
#include "timer.h" 
#include "ov2640.h" 
#include "dcmi.h" 
#include "string.h"
#include "pwm.h"




//JPEG尺寸支持列表
const u16 jpeg_img_size_tbl[][2]=
{
	176,144,	//QCIF
	160,120,	//QQVGA
	352,288,	//CIF
	320,240,	//QVGA
	640,480,	//VGA
	800,600,	//SVGA
	1024,768,	//XGA
	1280,1024,	//SXGA
	1600,1200,	//UXGA
}; 


void TIM3_Int_Init(u16 arr,u16 psc);

//RGB565测试
//RGB数据直接显示在LCD上面
/*void rgb565_test(void)
{ 
	u8 key;
	LCD_Clear(WHITE);
  POINT_COLOR=RED; 
	
  OV2640_ImageWin_Set((800-480)/2,(600-600)/2,480,600);
	OV2640_RGB565_Mode();	//RGB565模式
	My_DCMI_Init();			//DCMI配置
	DCMI_DMA_Init((u32)&LCD->LCD_RAM,1,DMA_MemoryDataSize_HalfWord,DMA_MemoryInc_Disable);//DCMI DMA配置  
 	OV2640_OutSize_Set(lcddev.width,600); 
	DCMI_Start(); 		//启动传输
	while(1)
	{ 
		key=KEY_Scan(0); 
		if(key)
		{ 
			DCMI_Stop(); //停止显示
			switch(key)
			{				    
				case KEY0_PRES:	//对比度设置
		
					break;
				case KEY1_PRES:	//饱和度Saturation
				
					break;
				case KEY2_PRES:	//特效设置				 
				
					break;
				case WKUP_PRES:		    
					
					break;
			}
			
			DCMI_Start();//重新开始传输
		} 
		delay_ms(10);		
	}    
} 

*/


u16 rgb_buf[156][208]; 
u16 gray;
extern u8 flag;
u16 hang=0;
u8 X_MAX,Y_MAX=0;    //小球的坐标信息
u8 X_MAX_LSAT, X_MIN_LSAT, Y_MAX_LSAT, Y_MIN_LSAT=0;   //上一次小球坐标位置信息
u8 X,Y=0;      //小球的质心信息
u8 X_MIN=210,Y_MIN=210;
int flag_pid=0; //是否稳定
int pid_count=0;
int PWM_X,PWM_Y=0;          //pid参数初始化

float Err_X,Err_Y=0;
float Err_X_LAST,Err_Y_LAST=0;
float Aim_X,Aim_Y=0;
float temp_Aim_X,temp_Aim_Y=0;
float Kp,Ki,Kd=0;
float Int_X,Int_Y=0;
int time=0;
int intern_time=0; //内部计时 
char strTime[4]="0"; 
u8 current_key=0;
u8 key=0;
int area_center[9][2] = {131,83,76,82,24,81,130,140,74,138,21,134,125,190,73,189,22,183};

int main(void)
{ 
    u16 i,j;
  int count=0;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	delay_init(168);  //初始化延时函数
	uart_init(115200);		//初始化串口波特率为115200 
	usart2_init(42,115200);		//初始化串口2波特率为115200
	LED_Init();					//初始化LED 
 	LCD_Init();					//LCD初始化  
 	KEY_Init();					//按键初始化 
	TIM3_Int_Init(200-1,8400-1);//10Khz计数,20ms中断一次
	
	TIM14_PWM_Init(10000-1,168-1);	//重装载值10000，所以PWM频率为 150hz.      //舵机预留
	TIM11_PWM_Init(10000-1,336-1);	//重装载值10000，所以PWM频率为 150hz.	
	
 	usmart_dev.init(84);		//初始化USMART
 	POINT_COLOR=RED;//设置字体为红色 
	LCD_ShowString(30,50,200,16,16,"2017/8/12");	  	 
	while(OV2640_Init())//初始化OV2640
	{
		LCD_ShowString(30,130,240,16,16,"OV2640 ERR");
		delay_ms(200);
	    LCD_Fill(30,130,239,170,WHITE);
		delay_ms(200);
	}
	LCD_ShowString(30,130,200,16,16,"OV2640 OK");  
    
    OV2640_OutSize_Set(208,156); 
    OV2640_RGB565_Mode();	//RGB565模式
    My_DCMI_Init();			//DCMI配置
    DCMI_DMA_Init((u32)rgb_buf,sizeof(rgb_buf)/4,DMA_MemoryDataSize_HalfWord,DMA_MemoryInc_Enable);//DCMI DMA配置
    DCMI_Start(); 		//启动传输
	  current_key=0;
	  time=0;
			TIM_SetCompare1(TIM14,9250);	//修改比较值，修改占空比
			TIM_SetCompare1(TIM11,9250);	//修改比较值，修改占空比
    while(1)
    {
			key=KEY_Scan(0);
      if(key)
			{
				current_key=key;
    		time=0;
				TIM_SetCompare1(TIM14,9250);	//修改比较值，修改占空比
				TIM_SetCompare1(TIM11,9250);	//修改比较值，修改占空比
				delay_ms(100);
				Int_X=0;
				Int_Y=0;
				flag_pid=0;
			}		

            hang=0;
            LCD_SetCursor(0,0);  
            LCD_WriteRAM_Prepare();		//开始写入GRAM
            for(i=0;i<156;i++)
            {
                for(j=0;j<208;j++)
                {
                    if(j==207)
                    {
                        hang++;
                        LCD_SetCursor(0,i+1);  
                        LCD_WriteRAM_Prepare();		//开始写入GRAM
                    }
                    gray=((rgb_buf[i][j]>>11)*19595+((rgb_buf[i][j]>>5)&0x3f)*38469 +(rgb_buf[i][j]&0x1f)*7472)>>16;
                    if(gray>=23)
                    {
											  
											  if(i>10&&i<149&&j<192&&j>64)
												{
											    if(i>X_MAX) X_MAX=i;
										     	if(i<X_MIN) X_MIN=i;											 
									      
											    if(j>Y_MAX) Y_MAX=j;
										     	if(j<Y_MIN) Y_MIN=j;
											 
												}
                        LCD->LCD_RAM=WHITE;
                    }
                    else
                    {										 
                        LCD->LCD_RAM=BLACK; 
                    }
                }
            }
						
					X_MAX_LSAT =	X_MAX;    //更新pid的real坐标信息 清除掉本次坐标用于再次遍历最大值 最小值
				  X_MIN_LSAT =	X_MIN;
				  Y_MAX_LSAT =	Y_MAX;
				  Y_MIN_LSAT =	Y_MIN;    
						
				  X_MAX=0;
				  X_MIN=210;
					Y_MAX=0;
				  Y_MIN=210;
						
					X=(X_MAX_LSAT+X_MIN_LSAT)/2;
				  Y=(Y_MAX_LSAT+Y_MIN_LSAT)/2;
					
					if(X_MAX_LSAT>144) 			TIM_SetCompare1(TIM11,9350);	
					if(X_MIN_LSAT<15) TIM_SetCompare1(TIM11,9150);	
					if(Y_MAX_LSAT>187) 			TIM_SetCompare1(TIM14,9350);	
					if(X_MIN_LSAT<70) TIM_SetCompare1(TIM14,9150);						
						
 					if(count==72)
					{	
					    printf("X=%d,Y=%d\r\n",X,Y);
//							printf("area2_x=%d,area2_y=%d\r\n",area_center[1][0],area_center[1][1]);

						printf("X_MAX_LSAT=%d,X_MIN_LSAT=%d,Y_MAX_LSAT=%d,Y_MIN_LSAT=%d\r\n",X_MAX_LSAT,X_MIN_LSAT,Y_MAX_LSAT,Y_MIN_LSAT);
              count=0;
					}
          count++;					
				}
}

void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3,TIM_IT_Update)==SET) //溢出中断
	{
    if(current_key == KEY0_PRES) //停留在区域2
		{
			  Kp=3;
			  Kd=75;
			  Ki=0.02;
			
			Aim_X=area_center[1][0]; //区域2圆心X坐标
			Aim_Y=area_center[1][1]; //区域2圆心Y坐标
		
			Err_X=X-Aim_X;
			Err_Y=Y-Aim_Y;
		  
			Int_X+=Err_X;
			Int_Y+=Err_Y;

			PWM_X=9250+(Err_X*Kp+(Err_X-Err_X_LAST)*Kd+Ki*Int_X);
			PWM_Y=9250+(Err_Y*Kp+(Err_Y-Err_Y_LAST)*Kd+Ki*Int_Y);
		
			if(PWM_Y>9350)PWM_Y=9350;
			if(PWM_Y<9150)PWM_Y=9150;
		
			if(PWM_X>9350)PWM_X=9350;
			if(PWM_X<9150)PWM_X=9150;
		
			Err_X_LAST=Err_X;
			Err_Y_LAST=Err_Y;

			TIM_SetCompare1(TIM14,PWM_Y);	//修改比较值，修改占空比
			TIM_SetCompare1(TIM11,PWM_X);	//修改比较值，修改占空比

		}
		if(current_key == KEY1_PRES)
		{
			  Kp=4.3;
			  Kd=75;
			  Ki=0;
			
			Aim_X=area_center[4][0]; //区域5圆心X坐标
			Aim_Y=area_center[4][1]; //区域5圆心Y坐标
		
			Err_X=X-Aim_X;
			Err_Y=Y-Aim_Y;
		  
			Int_X+=Err_X;
			Int_Y+=Err_Y;

			PWM_X=9250+(Err_X*Kp+(Err_X-Err_X_LAST)*Kd);
			PWM_Y=9250+(Err_Y*Kp+(Err_Y-Err_Y_LAST)*Kd);
		
			if(PWM_Y>9350)PWM_Y=9350;
			if(PWM_Y<9150)PWM_Y=9150;
		
			if(PWM_X>9350)PWM_X=9350;
			if(PWM_X<9150)PWM_X=9150;
		
			Err_X_LAST=Err_X;
			Err_Y_LAST=Err_Y;
//			PWM_X_LAST=PWM_X;
//			PWM_Y_LAST=PWM_Y;
		
			TIM_SetCompare1(TIM14,PWM_Y);	//修改比较值，修改占空比
			TIM_SetCompare1(TIM11,PWM_X);	//修改比较值，修改占空比

		}
				if(current_key == KEY2_PRES)
		{
			  Kp=4.4;
			  Kd=75;
			
			Aim_X=area_center[4][0]; //区域5圆心X坐标
			Aim_Y=area_center[4][1]; //区域5圆心Y坐标
		
			temp_Aim_X=area_center[3][0]; //区域4圆心X坐标
			temp_Aim_Y=area_center[3][1]; //区域4圆心Y坐标
			
			if(flag_pid==0)
			{
				Err_X=X-temp_Aim_X;
				Err_Y=Y-temp_Aim_Y;

			 PWM_X=9250+(Err_X*Kp+(Err_X-Err_X_LAST)*Kd);
			 PWM_Y=9250+(Err_Y*Kp+(Err_Y-Err_Y_LAST)*Kd);
			
				if(Err_X<3 && Err_X>-3 && (Err_Y-Err_Y_LAST)<15 && (Err_Y-Err_Y_LAST)>-15)
				{
					pid_count++;
				}
				if(pid_count==100)
				{
					flag_pid = 1;
					pid_count=0;
				}
			 if(PWM_Y>9350)PWM_Y=9350;
			 if(PWM_Y<9150)PWM_Y=9150;
		
			 if(PWM_X>9350)PWM_X=9350;
			 if(PWM_X<9150)PWM_X=9150;
		
			 Err_X_LAST=Err_X;
			 Err_Y_LAST=Err_Y;
		
			TIM_SetCompare1(TIM14,PWM_Y);	//修改比较值，修改占空比
			TIM_SetCompare1(TIM11,PWM_X);	//修改比较值，修改占空比
			}
			else
			{
				Err_X=X-Aim_X;
				Err_Y=Y-Aim_Y;

			 PWM_X=9250+(Err_X*Kp+(Err_X-Err_X_LAST)*Kd);
			 PWM_Y=9250+(Err_Y*Kp+(Err_Y-Err_Y_LAST)*Kd);
		
			 if(PWM_Y>9350)PWM_Y=9350;
			 if(PWM_Y<9150)PWM_Y=9150;
		
			 if(PWM_X>9350)PWM_X=9350;
			 if(PWM_X<9150)PWM_X=9150;
		
			 Err_X_LAST=Err_X;
			 Err_Y_LAST=Err_Y;
		
			TIM_SetCompare1(TIM14,PWM_Y);	//修改比较值，修改占空比
			TIM_SetCompare1(TIM11,PWM_X);	//修改比较值，修改占空比
			}
		}
		if(current_key == WKUP_PRES)
		{
			  Kp=0.84;
			  Kd=95;
//			  Ki=0.01;
			
			Aim_X=area_center[8][0]; //区域2圆心X坐标
			Aim_Y=area_center[8][1]; //区域2圆心Y坐标
		
			Err_X=X-Aim_X;
			Err_Y=Y-Aim_Y;
		  
//			Int_X+=Err_X;
//			Int_Y+=Err_Y;

			PWM_X=9250+(Err_X*Kp+(Err_X-Err_X_LAST)*Kd);
			PWM_Y=9250+(Err_Y*Kp+(Err_Y-Err_Y_LAST)*Kd);
		
			if(PWM_Y>9350)PWM_Y=9350;
			if(PWM_Y<9150)PWM_Y=9150;
		
			if(PWM_X>9350)PWM_X=9350;
			if(PWM_X<9150)PWM_X=9150;
		
			Err_X_LAST=Err_X;
			Err_Y_LAST=Err_Y;

			TIM_SetCompare1(TIM14,PWM_Y);	//修改比较值，修改占空比
			TIM_SetCompare1(TIM11,PWM_X);	//修改比较值，修改占空比

			}

		intern_time++;
		if(intern_time == 50)
		{	
			time++;
			sprintf(strTime,"%d",time);
			LCD_ShowString(30,180,200,16,16,"Time:");			
			LCD_ShowString(30,200,200,16,16,strTime);
			printf("pwm_x=%d,pwm_y=%d\r\n",PWM_X,PWM_Y);
			intern_time=0;
		}

	}
	TIM_ClearITPendingBit(TIM3,TIM_IT_Update);  //清除中断标志位
}








