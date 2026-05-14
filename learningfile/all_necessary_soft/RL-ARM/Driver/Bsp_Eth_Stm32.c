
/*************************************************
Copyright (C), 1988-1999, Tech. Co., Ltd.
File name: Bsp_Led.c
Author:
Version:
Date: PanLiyu  Rev1.0  2017-07-22
Description: 
1: RUN LED的IO初始化
2: RUN LED灯的主函数
3: 软件RTC的计时函数
Others: 
Function List: // 主要函数列表，每条记录应包括函数名及功能简要说明
1. ....
History: 修改的历史记录
*************************************************/
#include "includes.h"
#include "ETH_STM32x.h"
#include "Net_Config.h"
#include "stdio.h"

unsigned char G_ucEthLinkStatus = 0; /* 网络是否初始化成功  1:成功 0:失败 */

extern U8 own_hw_adr[];
/* Local variables */
U8 TxBufIndex=0;
U8 RxBufIndex=0;

/* ENET local DMA Descriptors. */
RX_Desc Rx_Desc[NUM_RX_BUF];
TX_Desc Tx_Desc[NUM_TX_BUF];

/* ENET local DMA buffers. */
U32 rx_buf[NUM_RX_BUF][ETH_BUF_SIZE>>2];
U32 tx_buf[NUM_TX_BUF][ETH_BUF_SIZE>>2];
/*--------------------------- read_PHY --------------------------------------*/

u16 read_PHY (u32 PhyReg) {
  /* Read a PHY register 'PhyReg'. */
  u32 tout;
  
  ETH->MACMIIAR = DP83848C_DEF_ADR << 11 | PhyReg << 6 | MMAR_MB;
  
  /* Wait until operation completed */
  tout = 0;
  for (tout = 0; tout < MII_RD_TOUT; tout++) {
    if ((ETH->MACMIIAR & MMAR_MB) == 0) {
      break;
    }
  }
  return (ETH->MACMIIDR & MMDR_MD);
}
/*--------------------------- write_PHY -------------------------------------*/

void write_PHY (u32 PhyReg, u16 Value) {
  /* Write a data 'Value' to PHY register 'PhyReg'. */
  u32 tout;
  
  ETH->MACMIIDR = Value;
  ETH->MACMIIAR = DP83848C_DEF_ADR << 11 | PhyReg << 6 | MMAR_MW | MMAR_MB;
  
  /* Wait utill operation completed */
  tout = 0;
  for (tout = 0; tout < MII_WR_TOUT; tout++) {
    if ((ETH->MACMIIAR & MMAR_MB) == 0) {
      break;
    }
  }
}
void Eth_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; //ETH_MDIO
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 ;    //ETH_MDC
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11 |GPIO_Pin_12 | GPIO_Pin_13;  
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    //  GPIO_PinRemapConfig(GPIO_Remap_ETH, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 ;       //RMII_REF_CLK
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;// RMII_RXD0 RMII_RXD1
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;//RMII_CRS_DV 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8; //RMII_MC0
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}
void Eth_Gpio_Init(void)
{
    NVIC_InitTypeDef   NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = ETH_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_ETH_MAC | RCC_AHBPeriph_ETH_MAC_Tx | RCC_AHBPeriph_ETH_MAC_Rx, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC| RCC_APB2Periph_AFIO , ENABLE);	 
    Eth_GPIO_Config();
    GPIO_ETH_MediaInterfaceConfig(GPIO_ETH_MediaInterface_RMII);
    RCC_MCOConfig(RCC_MCO_HSE);//(RCC_MCO_PLL3CLK);
}

void rx_descr_init (void) {
  /* Initialize Receive DMA Descriptor array. */
  U32 i,next;
  RxBufIndex = 0;
  for (i = 0, next = 0; i < NUM_RX_BUF; i++)
  {
    if (++next == NUM_RX_BUF) next = 0;
    Rx_Desc[i].Stat = DMA_RX_OWN;
    Rx_Desc[i].Ctrl = DMA_RX_RCH|ETH_BUF_SIZE;
    Rx_Desc[i].Addr = (U32)&rx_buf[i][0];
    Rx_Desc[i].Next = (U32)&Rx_Desc[next];
  }
  ETH->DMARDLAR = (U32)&Rx_Desc[0];		
}

void tx_descr_init (void) {
  /* Initialize Transmit DMA Descriptor array. */
  U32 i,next;
  TxBufIndex = 0;
  for (i = 0, next = 0; i < NUM_TX_BUF; i++) {
    if (++next == NUM_TX_BUF) next = 0;
    Tx_Desc[i].CtrlStat = DMA_TX_TCH|DMA_TX_LS|DMA_TX_FS;
    Tx_Desc[i].Addr     = (U32)&tx_buf[i][0];
    Tx_Desc[i].Next     = (U32)&Tx_Desc[next];
  }
  ETH->DMATDLAR = (U32)&Tx_Desc[0];
}
void init_ethernet (void) 
{
    unsigned char Phy_Id=0;
    unsigned char Eth_Sign = 0;/* 网络是否初始化成功标志 */
    
    U32 regv,tout,id1,id2; 
    Eth_Gpio_Init();

    // Reset Ethernet MAC
    RCC->AHBRSTR  |= 0x00004000;
    RCC->AHBRSTR  &=~0x00004000;
    ETH->DMABMR  |= DBMR_SR;
    while (ETH->DMABMR & DBMR_SR){;}

    /* MDC Clock range 60-72MHz. */
    ETH->MACMIIAR = 0x00000000;
    /* Put the DP83848C in reset mode */
    write_PHY (PHY_REG_BMCR, 0x8000);

    /* Wait for hardware reset to end. */
    for (tout = 0; tout < 0x10000; tout++) 
    {
        regv = read_PHY (PHY_REG_BMCR);
        if (!(regv & 0x8800))
        {
            Eth_Sign |= 1<<0;
            break;
        }
    }
    tout = 0;
    id1 = read_PHY (PHY_REG_IDR1);
    id2 = read_PHY (PHY_REG_IDR2);
    tout = ((id1 << 16) | (id2 & 0xFFF0));
    if(tout == DP83848C_ID)  Phy_Id = 0;
    else Phy_Id = 1;

    write_PHY (PHY_REG_BMCR, PHY_AUTO_NEG);
    //  write_PHY (PHY_REG_RBR, 0x0030); LAN8720AI 不需要这个寄存器
    /* Wait to complete Auto_Negotiation. */
    for (tout = 0; tout < 0x10000; tout++) //开启自动协商功能
    {
        regv = read_PHY (PHY_REG_BMSR);
        if (regv & 0x0020)
        {
            Eth_Sign |= 1<<1;
            break;
        }
    }

    regv = 0;
    if(Phy_Id == 0) //读状态寄存器  判定LINK STATUS
    {
        for (tout = 0; tout < 0x10000; tout++) 
        {
            regv = read_PHY (PHY_REG_STS);
            if((regv & 0x0001)!=0)  
            {  
                Eth_Sign |= 1<<2;
                break;
            }
        }
    }
    else if(Phy_Id == 1) //读状态寄存器  判定LINK STATUS
    {
        for (tout = 0; tout < 0x10000; tout++) 
        {
            regv = read_PHY (0x01);  
            if((regv & 0x0004)!=0) 
            { 
                Eth_Sign |= 1<<2;
                break;
            }
        }
        regv = read_PHY (0x00);//读DUPLEX MODE  
    }
    if(Eth_Sign == 0x07)
    {
        G_ucEthLinkStatus = 1;/* 网络初始化成功 */
    }
    
    /* Initialize MAC control register */
    ETH->MACCR  = MCR_ROD;
    /* Configure Full/Half Duplex mode. */
    if(Phy_Id == 0)//根据协商来处理是全双工还是半双工
    {
        if (regv & 0x0004) 
        {
            /* Full duplex is enabled. */
            ETH->MACCR |= MCR_DM;
        }
        /* Configure 100MBit/10MBit mode. */
        if ((regv & 0x0002) == 0) //设置速度
        {
            /* 100MBit mode. */
            ETH->MACCR |= MCR_FES;
        }		
    }
    else if(Phy_Id == 1)
    {
        if (regv & 0x0100) 
        {
            /* Full duplex is enabled. */
            ETH->MACCR |= MCR_DM;
        }
        /* Configure 100MBit/10MBit mode. */
        //		if ((regv & 0x0002) == 0) 
        //		{
        /* 100MBit mode. */
        ETH->MACCR |= MCR_FES;
        //		}
    }

    /* MAC address filtering, accept multicast packets. */
    ETH->MACFFR = MFFR_HPF | MFFR_PAM;
    ETH->MACFCR = MFCR_ZQPD;
    /* Set the Ethernet MAC Address registers */
    ETH->MACA0HR = ((U32)own_hw_adr[5] <<  8) | (U32)own_hw_adr[4];
    ETH->MACA0LR = ((U32)own_hw_adr[3] << 24) | (U32)own_hw_adr[2] << 16 |
    ((U32)own_hw_adr[1] <<  8) | (U32)own_hw_adr[0]; 
    /* Initialize Tx and Rx DMA Descriptors */
    rx_descr_init ();
    tx_descr_init ();
    /* Flush FIFO, start DMA Tx and Rx */
    ETH->DMAOMR = DOMR_FTF | DOMR_ST | DOMR_SR;
    /* Enable receiver and transmiter */
    ETH->MACCR |= MCR_TE | MCR_RE;
    /* Reset all interrupts */
    ETH->DMASR  = 0xFFFFFFFF;
    /* Enable Rx and NIS interrupts. */
    ETH->DMAIER = INT_NISE | INT_RIE|ETH_DMAIER_AISE;
}
/*--------------------------- int_enable_eth ---------------------------------*/

void int_enable_eth (void) {
  /* Ethernet Interrupt Enable function. */
  NVIC->ISER[1] = 1 << 29;
}


/*--------------------------- int_disable_eth --------------------------------*/
extern void Usart1_SendBuff(unsigned char *str,unsigned short len);
void int_disable_eth (void) {
  /* Ethernet Interrupt Disable function. */
  NVIC->ICER[1] = 1 << 29;
}

//发送数据
/*--------------------------- send_frame -------------------------------------*/
void send_frame (OS_FRAME *frame) {
  /* Send frame to ETH ethernet controller */
  U32 *sp,*dp;
  U32 i,j;

  j = TxBufIndex;
  /* Wait until previous packet transmitted. */
  while (Tx_Desc[j].CtrlStat & DMA_TX_OWN);

  sp = (U32 *)&frame->data[0];
  dp = (U32 *)(Tx_Desc[j].Addr & ~3);

  /* Copy frame data to ETH IO buffer. */
  for (i = (frame->length + 3) >> 2; i; i--) {
    *dp++ = *sp++;
  }
  Tx_Desc[j].Size      = frame->length;
  Tx_Desc[j].CtrlStat |= DMA_TX_OWN;
  if (++j == NUM_TX_BUF) j = 0;
  TxBufIndex = j;
  /* Start frame transmission. */
  ETH->DMASR   = DSR_TPSS;
  ETH->DMATPDR = 0;
}

//中断接收
void ETH_IRQHandler (void) {
  /* Ethernet Controller Interrupt function. */
  OS_FRAME *frame;
  U32 RxLen;//
//	unsigned char NeedDo_Flag = 0;
  unsigned char i;
  U32 *sp,*dp;
  i = RxBufIndex;
  do 
  {
    /* Valid frame has been received. */
    if (Rx_Desc[i].Stat & DMA_RX_ERROR_MASK) 
    {
      goto rel;
    }
    if ((Rx_Desc[i].Stat & DMA_RX_FS)==0) 
    {
      goto rel;
    }
    if ((Rx_Desc[i].Stat & DMA_RX_LS)==0) 
    {
      goto rel;
    }
    RxLen = ((Rx_Desc[i].Stat >> 16) & 0x3FFF) - 4;
    if (RxLen > ETH_MTU) 
    {
      /* Packet too big, ignore it and free buffer. */
      goto rel;
    }
    /* Flag 0x80000000 to skip sys_error() call when out of memory. */
    frame = alloc_mem (RxLen | 0x80000000);
    /* if 'alloc_mem()' has failed, ignore this packet. */
    if (frame != NULL) {
      sp = (U32 *)(Rx_Desc[i].Addr & ~3);
      dp = (U32 *)&frame->data[0];
      for (RxLen = (RxLen + 3) >> 2; RxLen; RxLen--) {
        *dp++ = *sp++;
      }
//			NeedDo_Flag = 1;
      put_in_queue (frame);
    }
    /* Release this frame from ETH IO buffer. */
  rel:
    Rx_Desc[i].Stat =0xffffffff;// DMA_RX_OWN;
    if (++i == NUM_RX_BUF) i = 0;
  } 
  while ((Rx_Desc[i].Stat & DMA_RX_OWN) == 0);
  RxBufIndex = i;
  
  if (ETH->DMASR & INT_RBUIE) {
    /* Rx DMA suspended, resume DMA reception. */
    ETH->DMASR   = INT_RBUIE;
    ETH->DMARPDR = 0;
  }
  /* Clear the interrupt pending bits. */
  ETH->DMASR = INT_NISE|INT_RIE;
}

//读LAN8720A的网络状态
unsigned char Phy_Link_Stata(void)
{
    u16 regv; 
    regv = read_PHY (0x01); 
    if((regv & 0x0004)==0)return 0;
    return 1;
}


