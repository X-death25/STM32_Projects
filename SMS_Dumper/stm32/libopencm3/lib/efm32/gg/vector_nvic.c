/* This file is part of the libopencm3 project.
 *
 * It was generated by the irq2nvic_h script.
 *
 * This part needs to get included in the compilation unit where
 * blocking_handler gets defined due to the way #pragma works.
 */


/** @defgroup CM3_nvic_isrpragmas_EFM32GG User interrupt service routines (ISR) defaults for EFM32 Giant Gecko series
    @ingroup CM3_nvic_isrpragmas

    @{*/

#pragma weak dma_isr = blocking_handler
#pragma weak gpio_even_isr = blocking_handler
#pragma weak timer0_isr = blocking_handler
#pragma weak usart0_rx_isr = blocking_handler
#pragma weak usart0_tx_isr = blocking_handler
#pragma weak usb_isr = blocking_handler
#pragma weak acmp01_isr = blocking_handler
#pragma weak adc0_isr = blocking_handler
#pragma weak dac0_isr = blocking_handler
#pragma weak i2c0_isr = blocking_handler
#pragma weak i2c1_isr = blocking_handler
#pragma weak gpio_odd_isr = blocking_handler
#pragma weak timer1_isr = blocking_handler
#pragma weak timer2_isr = blocking_handler
#pragma weak timer3_isr = blocking_handler
#pragma weak usart1_rx_isr = blocking_handler
#pragma weak usart1_tx_isr = blocking_handler
#pragma weak lesense_isr = blocking_handler
#pragma weak usart2_rx_isr = blocking_handler
#pragma weak usart2_tx_isr = blocking_handler
#pragma weak uart0_rx_isr = blocking_handler
#pragma weak uart0_tx_isr = blocking_handler
#pragma weak uart1_rx_isr = blocking_handler
#pragma weak uart1_tx_isr = blocking_handler
#pragma weak leuart0_isr = blocking_handler
#pragma weak leuart1_isr = blocking_handler
#pragma weak letimer0_isr = blocking_handler
#pragma weak pcnt0_isr = blocking_handler
#pragma weak pcnt1_isr = blocking_handler
#pragma weak pcnt2_isr = blocking_handler
#pragma weak rtc_isr = blocking_handler
#pragma weak burtc_isr = blocking_handler
#pragma weak cmu_isr = blocking_handler
#pragma weak vcmp_isr = blocking_handler
#pragma weak lcd_isr = blocking_handler
#pragma weak msc_isr = blocking_handler
#pragma weak aes_isr = blocking_handler
#pragma weak ebi_isr = blocking_handler

/**@}*/

/* Initialization template for the interrupt vector table. This definition is
 * used by the startup code generator (vector.c) to set the initial values for
 * the interrupt handling routines to the chip family specific _isr weak
 * symbols. */

#define IRQ_HANDLERS \
    [NVIC_DMA_IRQ] = dma_isr, \
    [NVIC_GPIO_EVEN_IRQ] = gpio_even_isr, \
    [NVIC_TIMER0_IRQ] = timer0_isr, \
    [NVIC_USART0_RX_IRQ] = usart0_rx_isr, \
    [NVIC_USART0_TX_IRQ] = usart0_tx_isr, \
    [NVIC_USB_IRQ] = usb_isr, \
    [NVIC_ACMP01_IRQ] = acmp01_isr, \
    [NVIC_ADC0_IRQ] = adc0_isr, \
    [NVIC_DAC0_IRQ] = dac0_isr, \
    [NVIC_I2C0_IRQ] = i2c0_isr, \
    [NVIC_I2C1_IRQ] = i2c1_isr, \
    [NVIC_GPIO_ODD_IRQ] = gpio_odd_isr, \
    [NVIC_TIMER1_IRQ] = timer1_isr, \
    [NVIC_TIMER2_IRQ] = timer2_isr, \
    [NVIC_TIMER3_IRQ] = timer3_isr, \
    [NVIC_USART1_RX_IRQ] = usart1_rx_isr, \
    [NVIC_USART1_TX_IRQ] = usart1_tx_isr, \
    [NVIC_LESENSE_IRQ] = lesense_isr, \
    [NVIC_USART2_RX_IRQ] = usart2_rx_isr, \
    [NVIC_USART2_TX_IRQ] = usart2_tx_isr, \
    [NVIC_UART0_RX_IRQ] = uart0_rx_isr, \
    [NVIC_UART0_TX_IRQ] = uart0_tx_isr, \
    [NVIC_UART1_RX_IRQ] = uart1_rx_isr, \
    [NVIC_UART1_TX_IRQ] = uart1_tx_isr, \
    [NVIC_LEUART0_IRQ] = leuart0_isr, \
    [NVIC_LEUART1_IRQ] = leuart1_isr, \
    [NVIC_LETIMER0_IRQ] = letimer0_isr, \
    [NVIC_PCNT0_IRQ] = pcnt0_isr, \
    [NVIC_PCNT1_IRQ] = pcnt1_isr, \
    [NVIC_PCNT2_IRQ] = pcnt2_isr, \
    [NVIC_RTC_IRQ] = rtc_isr, \
    [NVIC_BURTC_IRQ] = burtc_isr, \
    [NVIC_CMU_IRQ] = cmu_isr, \
    [NVIC_VCMP_IRQ] = vcmp_isr, \
    [NVIC_LCD_IRQ] = lcd_isr, \
    [NVIC_MSC_IRQ] = msc_isr, \
    [NVIC_AES_IRQ] = aes_isr, \
    [NVIC_EBI_IRQ] = ebi_isr
