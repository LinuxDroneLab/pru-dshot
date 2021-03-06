#include <include/resource_table.h>
#include <stdint.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <pru_rpmsg.h>
#include <pru_rpmsg_lib.h>
#include <pru_dshot_driver.h>

#define VIRTIO_CONFIG_S_DRIVER_OK       4


/**
 * main.c
 */
int main(void)
{
    PruRpmsgLibConfig rpmsgConfig;
    PruDShotLibConfig pwmssConfig = {0};

    volatile unsigned char *status;

    /* Allow OCP master port access by the PRU so the PRU can read external memories */
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    /* Clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us */
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

    /* Make sure the Linux drivers are ready for RPMsg communication */
    /* this is another place where a hang could occur */
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK))
        ;

    /* Initialize the RPMsg transport structure */
    /* this function is defined in rpmsg_pru.c.  It's sole purpose is to call pru_virtqueue_init twice (once for
     vring0 and once for vring1).  pru_virtqueue_init is defined in pru_virtqueue.c.  It's sole purpose is to
     call vring_init.  Not sure yet where that's defined, but it appears to be part of the pru_rpmsg iface.*/
    /* should probably test for RPMSG_SUCCESS.  If not, then the interface is not working and code should halt */
    pru_rpmsg_init(&rpmsgConfig.transport, &resourceTable.rpmsg_vring0,
                   &resourceTable.rpmsg_vring1, TO_ARM_HOST, FROM_ARM_HOST);

    /* Create the RPMsg channel between the PRU and ARM user space using the transport structure. */
    // In a real-time environment, rather than waiting forever, this can probably be run loop-after-loop
    // until success is achieved.  At that point, set a flag and then enable the send/receive functionality
    while (pru_rpmsg_channel(RPMSG_NS_CREATE, &rpmsgConfig.transport, CHAN_NAME, CHAN_DESC,
    CHAN_PORT) != PRU_RPMSG_SUCCESS)
        ;


    /* Compose RC lib with RPMSG lib */
    pwmssConfig.onStart = pru_rpmsg_lib_Send;
    pwmssConfig.onStop = pru_rpmsg_lib_Send;
    pwmssConfig.onSetData = pru_rpmsg_lib_Send;
    pwmssConfig.onSetDuty = pru_rpmsg_lib_Send;
    pwmssConfig.onSetPeriod = pru_rpmsg_lib_Send;
    pru_dshot_lib_Conf(&pwmssConfig);

    rpmsgConfig.onReceived = pru_dshot_lib_ExecCmd;
    rpmsgConfig.hostInt = HOST_INT;
    rpmsgConfig.fromArmHost = FROM_ARM_HOST;
    pru_rpmsg_lib_Conf(&rpmsgConfig);

    pru_dshot_lib_Init(0);
    pru_dshot_lib_Init(1);
    pru_dshot_lib_Init(2);
    pru_dshot_lib_Start(0);

    // Run
    while (1)
    {
        pru_dshot_lib_Pulse(0);
//        pru_dshot_lib_Pulse(1);
//        pru_dshot_lib_Pulse(2);
//        pru_rpmsg_lib_Pulse();
    }
}
