#ifndef FBLTRANSPORT_CFG_H
#define FBLTRANSPORT_CFG_H

/* Shared transport sizing. Both the Ethernet TCP stream and the DoIP receive
 * frame buffer hold one complete maximum-size DoIP TransferData frame. */
#define FBL_DOIP_HEADER_SIZE              8u
#define FBL_DOIP_DIAG_ADDRESS_SIZE        4u
#define FBL_TRANSFER_DATA_BYTES           32768u
#define FBL_UDS_TRANSFER_OVERHEAD         2u
#define FBL_UDS_MAX_BLOCK_LENGTH          (FBL_TRANSFER_DATA_BYTES + FBL_UDS_TRANSFER_OVERHEAD)
#define FBL_DOIP_MAX_TRANSFER_FRAME_SIZE  (FBL_DOIP_HEADER_SIZE + FBL_DOIP_DIAG_ADDRESS_SIZE + FBL_UDS_MAX_BLOCK_LENGTH)
#define FBL_DOIP_TCP_RX_STREAM_SIZE       32800u

#define FBL_ETH_UDP_RX_QUEUE_DEPTH_CFG    4u
#define FBL_ETH_UDP_RX_BUFFER_SIZE        64u

#if FBL_DOIP_MAX_TRANSFER_FRAME_SIZE != 32782u
#error "Unexpected DoIP TransferData frame size"
#endif

#if FBL_DOIP_TCP_RX_STREAM_SIZE < FBL_DOIP_MAX_TRANSFER_FRAME_SIZE
#error "DoIP TCP stream cannot hold one complete TransferData frame"
#endif

#endif
