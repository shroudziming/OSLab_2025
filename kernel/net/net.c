#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    int translen = 0;
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // translen = e1000_transmit(txpacket,length);

    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full
    while(1){
        translen = e1000_transmit(txpacket,length);
        if(translen == 0){
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE); //enable TXQE interrupt
            do_block(&current_running[cpu_id]->list, &send_block_queue);
        }else{
            break;
        }
    }

    return translen;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    if(pkt_num <= 0) {
        return 0;
    }
    int recvlens = 0;
    for(int i = 0; i < pkt_num;) {
        pkt_lens[i] = e1000_poll(rxbuffer);
        if(pkt_lens[i] == 0){
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0); //enable RXDMT0 interrupt
            local_flush_dcache();
            do_block(&current_running[cpu_id]->list, &recv_block_queue);
            continue;
        }
        rxbuffer += pkt_lens[i];
        recvlens += pkt_lens[i];
        i++;
    }
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return recvlens;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
    e1000_handle_irq();
}


void e1000_handle_irq(void)
{
    local_flush_dcache();
    uint32_t cause = e1000_read_reg(e1000, E1000_ICR);
    if(cause & E1000_ICR_TXQE){
        //unblock send queue
        free_block_list(&send_block_queue);
        //TXQE interrupt shut
        e1000_write_reg(e1000, E1000_IMC, E1000_IMS_TXQE);
        local_flush_dcache();
    }
    if(cause & E1000_ICR_RXDMT0){
        //release recv blocked pcbs
        free_block_list(&recv_block_queue);
        e1000_write_reg(e1000, E1000_IMC, E1000_IMS_RXDMT0);
        local_flush_dcache();
    }
}