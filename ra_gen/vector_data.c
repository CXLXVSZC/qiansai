/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = ceu_isr, /* CEU CEUI (CEU interrupt) */
            [1] = sci_b_uart_rxi_isr, /* SCI8 RXI (Receive data full) */
            [2] = sci_b_uart_txi_isr, /* SCI8 TXI (Transmit data empty) */
            [3] = sci_b_uart_tei_isr, /* SCI8 TEI (Transmit end) */
            [4] = sci_b_uart_eri_isr, /* SCI8 ERI (Receive error) */
            [5] = glcdc_line_detect_isr, /* GLCDC LINE DETECT (Specified line) */
            [6] = mipi_dsi_seq0_isr, /* MIPIDSI SEQ0 (Sequence operation channel 0 interrupt) */
            [7] = mipi_dsi_seq1_isr, /* MIPIDSI SEQ1 (Sequence operation channel 1 interrupt) */
            [8] = mipi_dsi_vin1_isr, /* MIPIDSI VIN1 (Video-Input operation channel1 interrupt) */
            [9] = mipi_dsi_rcv_isr, /* MIPIDSI RCV (DSI packet receive interrupt) */
            [10] = mipi_dsi_ferr_isr, /* MIPIDSI FERR (DSI fatal error interrupt) */
            [11] = mipi_dsi_ppi_isr, /* MIPIDSI PPI (DSI D-PHY PPI interrupt) */
            [12] = drw_int_isr, /* DRW INT (DRW interrupt) */
            [13] = rm_ethosu_isr, /* NPU IRQ (NPU IRQ) */
            [14] = canfd_error_isr, /* CAN0 CHERR (Channel  error) */
            [15] = canfd_channel_tx_isr, /* CAN0 TX (Transmit interrupt) */
            [16] = canfd_common_fifo_rx_isr, /* CAN0 COMFRX (Common FIFO receive interrupt) */
            [17] = canfd_error_isr, /* CAN GLERR (Global error) */
            [18] = canfd_rx_fifo_isr, /* CAN RXF (Global receive FIFO interrupt) */
            [19] = canfd_error_isr, /* CAN1 CHERR (Channel  error) */
            [20] = canfd_channel_tx_isr, /* CAN1 TX (Transmit interrupt) */
            [21] = canfd_common_fifo_rx_isr, /* CAN1 COMFRX (Common FIFO receive interrupt) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_CEU_CEUI,GROUP0), /* CEU CEUI (CEU interrupt) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_SCI8_RXI,GROUP1), /* SCI8 RXI (Receive data full) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_SCI8_TXI,GROUP2), /* SCI8 TXI (Transmit data empty) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_SCI8_TEI,GROUP3), /* SCI8 TEI (Transmit end) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_SCI8_ERI,GROUP4), /* SCI8 ERI (Receive error) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_GLCDC_LINE_DETECT,GROUP5), /* GLCDC LINE DETECT (Specified line) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_SEQ0,GROUP6), /* MIPIDSI SEQ0 (Sequence operation channel 0 interrupt) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_SEQ1,GROUP7), /* MIPIDSI SEQ1 (Sequence operation channel 1 interrupt) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_VIN1,GROUP0), /* MIPIDSI VIN1 (Video-Input operation channel1 interrupt) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_RCV,GROUP1), /* MIPIDSI RCV (DSI packet receive interrupt) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_FERR,GROUP2), /* MIPIDSI FERR (DSI fatal error interrupt) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_MIPIDSI_PPI,GROUP3), /* MIPIDSI PPI (DSI D-PHY PPI interrupt) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_DRW_INT,GROUP4), /* DRW INT (DRW interrupt) */
            [13] = BSP_PRV_VECT_ENUM(EVENT_NPU_IRQ,GROUP5), /* NPU IRQ (NPU IRQ) */
            [14] = BSP_PRV_VECT_ENUM(EVENT_CAN0_CHERR,GROUP6), /* CAN0 CHERR (Channel  error) */
            [15] = BSP_PRV_VECT_ENUM(EVENT_CAN0_TX,GROUP7), /* CAN0 TX (Transmit interrupt) */
            [16] = BSP_PRV_VECT_ENUM(EVENT_CAN0_COMFRX,GROUP0), /* CAN0 COMFRX (Common FIFO receive interrupt) */
            [17] = BSP_PRV_VECT_ENUM(EVENT_CAN_GLERR,GROUP1), /* CAN GLERR (Global error) */
            [18] = BSP_PRV_VECT_ENUM(EVENT_CAN_RXF,GROUP2), /* CAN RXF (Global receive FIFO interrupt) */
            [19] = BSP_PRV_VECT_ENUM(EVENT_CAN1_CHERR,GROUP3), /* CAN1 CHERR (Channel  error) */
            [20] = BSP_PRV_VECT_ENUM(EVENT_CAN1_TX,GROUP4), /* CAN1 TX (Transmit interrupt) */
            [21] = BSP_PRV_VECT_ENUM(EVENT_CAN1_COMFRX,GROUP5), /* CAN1 COMFRX (Common FIFO receive interrupt) */
        };
        #endif
        #endif