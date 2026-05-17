#include "lwip_geth_conf.h"

#if (PHY_DEVICE_NAME == PHY_DP83825I)

#include "lwip_geth_private_phy_dp83825i.h"

#define DP83825I_PHY_ADDR              0u

#define DP83825I_REG_BMCR              0x00u
#define DP83825I_REG_BMSR              0x01u
#define DP83825I_REG_PHYIDR1           0x02u
#define DP83825I_REG_PHYIDR2           0x03u
#define DP83825I_REG_ANAR              0x04u
#define DP83825I_REG_PHYSTS            0x10u

#define DP83825I_BMCR_RESET            0x8000u
#define DP83825I_BMCR_AUTONEG_ENABLE   0x1000u
#define DP83825I_BMCR_RESTART_AUTONEG  0x0200u

#define DP83825I_BMSR_LINK_STATUS      0x0004u
#define DP83825I_BMSR_AUTONEG_DONE     0x0020u

#define DP83825I_PHYSTS_LINK           0x0001u
#define DP83825I_PHYSTS_SPEED_10       0x0002u
#define DP83825I_PHYSTS_FULL_DUPLEX    0x0004u

#define DP83825I_MDIO_MAX_WAIT         100000u
#define DP83825I_RESET_TIMEOUT_100MS   20u
#define DP83825I_AUTONEG_TIMEOUT_100MS 50u

static lwip_geth_PhyDp83825i_StatusType Dp83825i_Status;

volatile uint32 Dp83825i_DebugPhyScanAddr;
volatile uint32 Dp83825i_DebugPhyAddrFound = 0xFFFFFFFFu;
volatile uint32 Dp83825i_DebugPhyId1;
volatile uint32 Dp83825i_DebugPhyId2;
volatile uint32 Dp83825i_DebugBmcr;
volatile uint32 Dp83825i_DebugBmsr;
volatile uint32 Dp83825i_DebugReadOkCount;
volatile uint32 Dp83825i_DebugReadFailCount;

void Dp83825i_DebugScanMdio(void)
{
    uint32 addr;
    uint32 id1;
    uint32 id2;
    uint32 bmcr;
    uint32 bmsr;

    Dp83825i_DebugPhyAddrFound = 0xFFFFFFFFu;
    Dp83825i_DebugPhyId1 = 0u;
    Dp83825i_DebugPhyId2 = 0u;
    Dp83825i_DebugBmcr = 0u;
    Dp83825i_DebugBmsr = 0u;

    for (addr = 0u; addr < 32u; addr++)
    {
        Dp83825i_DebugPhyScanAddr = addr;

        if (lwip_geth_private_Phy_Dp83825i_read_mdio_reg(addr, DP83825I_REG_PHYIDR1, &id1) == 0u)
        {
            Dp83825i_DebugReadFailCount++;
            continue;
        }

        if (lwip_geth_private_Phy_Dp83825i_read_mdio_reg(addr, DP83825I_REG_PHYIDR2, &id2) == 0u)
        {
            Dp83825i_DebugReadFailCount++;
            continue;
        }

        Dp83825i_DebugReadOkCount++;

        if ((id1 != 0x0000u) && (id1 != 0xFFFFu) &&
            (id2 != 0x0000u) && (id2 != 0xFFFFu))
        {
            Dp83825i_DebugPhyAddrFound = addr;
            Dp83825i_DebugPhyId1 = id1;
            Dp83825i_DebugPhyId2 = id2;

            (void)lwip_geth_private_Phy_Dp83825i_read_mdio_reg(addr, DP83825I_REG_BMCR, &bmcr);
            (void)lwip_geth_private_Phy_Dp83825i_read_mdio_reg(addr, DP83825I_REG_BMSR, &bmsr);

            Dp83825i_DebugBmcr = bmcr;
            Dp83825i_DebugBmsr = bmsr;
            break;
        }
    }
}

static uint32 Dp83825i_WaitMdioReady(void)
{
    uint32 timeout = DP83825I_MDIO_MAX_WAIT;

    while ((GETH_MAC_MDIO_ADDRESS.B.GB != 0u) && (timeout > 0u))
    {
        timeout--;
    }

    return (timeout > 0u) ? 1u : 0u;
}

uint32 lwip_geth_private_Phy_Dp83825i_read_mdio_reg(uint32 layeraddr, uint32 regaddr, uint32 *pdata)
{
    if (pdata == 0)
    {
        Dp83825i_Status.mdioErrorCnt++;
        return 0u;
    }

    if (Dp83825i_WaitMdioReady() == 0u)
    {
        Dp83825i_Status.mdioErrorCnt++;
        return 0u;
    }

    GETH_MAC_MDIO_ADDRESS.U =
        ((layeraddr & 0x1Fu) << 21) |
        ((regaddr   & 0x1Fu) << 16) |
        (0u << 8) |
        (3u << 2) |
        (1u << 0);

    if (Dp83825i_WaitMdioReady() == 0u)
    {
        Dp83825i_Status.mdioErrorCnt++;
        return 0u;
    }

    *pdata = GETH_MAC_MDIO_DATA.U & 0xFFFFu;
    return 1u;
}

uint32 lwip_geth_private_Phy_Dp83825i_write_mdio_reg(uint32 layeraddr, uint32 regaddr, uint32 data)
{
    if (Dp83825i_WaitMdioReady() == 0u)
    {
        Dp83825i_Status.mdioErrorCnt++;
        return 0u;
    }

    GETH_MAC_MDIO_DATA.U = data & 0xFFFFu;

    GETH_MAC_MDIO_ADDRESS.U =
        ((layeraddr & 0x1Fu) << 21) |
        ((regaddr   & 0x1Fu) << 16) |
        (0u << 8) |
        (1u << 2) |
        (1u << 0);

    if (Dp83825i_WaitMdioReady() == 0u)
    {
        Dp83825i_Status.mdioErrorCnt++;
        return 0u;
    }

    return 1u;
}

void lwip_geth_private_Phy_Dp83825i_reset(void)
{
    Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_RESET;
    Dp83825i_Status.initDone = 0u;
    Dp83825i_Status.linkUp = 0u;
    Dp83825i_Status.autonegDone = 0u;
    Dp83825i_Status.resetTimeoutCnt = 0u;
    Dp83825i_Status.autonegTimeoutCnt = 0u;

    (void)lwip_geth_private_Phy_Dp83825i_write_mdio_reg(
        DP83825I_PHY_ADDR,
        DP83825I_REG_BMCR,
        DP83825I_BMCR_RESET);
}

uint32 lwip_geth_private_Phy_Dp83825i_init(void)
{
    Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_RESET;
    Dp83825i_Status.initDone = 0u;
    Dp83825i_Status.linkUp = 0u;
    Dp83825i_Status.autonegDone = 0u;
    Dp83825i_Status.speed100 = 1u;
    Dp83825i_Status.fullDuplex = 1u;
    Dp83825i_Status.resetTimeoutCnt = 0u;
    Dp83825i_Status.autonegTimeoutCnt = 0u;
    Dp83825i_Status.mdioErrorCnt = 0u;
    Dp83825i_Status.linkDownCnt = 0u;
    Dp83825i_Status.linkUpCnt = 0u;

    lwip_geth_private_Phy_Dp83825i_reset();

    return 1u;
}

void lwip_geth_private_Phy_Dp83825i_mainFunction_100ms(void)
{
    uint32 bmcr;
    uint32 bmsr;
    uint32 physts;

    switch (Dp83825i_Status.state)
    {
        case LWIP_GETH_PHY_DP83825I_STATE_RESET:
        {
            Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_WAIT_RESET_DONE;
            break;
        }

        case LWIP_GETH_PHY_DP83825I_STATE_WAIT_RESET_DONE:
        {
            if (lwip_geth_private_Phy_Dp83825i_read_mdio_reg(DP83825I_PHY_ADDR, DP83825I_REG_BMCR, &bmcr) == 0u)
            {
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_ERROR;
                break;
            }

            if ((bmcr & DP83825I_BMCR_RESET) == 0u)
            {
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_CONFIGURE;
            }
            else
            {
                Dp83825i_Status.resetTimeoutCnt++;

                if (Dp83825i_Status.resetTimeoutCnt >= DP83825I_RESET_TIMEOUT_100MS)
                {
                    Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_ERROR;
                }
            }

            break;
        }

        case LWIP_GETH_PHY_DP83825I_STATE_CONFIGURE:
        {
            (void)lwip_geth_private_Phy_Dp83825i_write_mdio_reg(
                DP83825I_PHY_ADDR,
                DP83825I_REG_ANAR,
                0x01E1u);

            (void)lwip_geth_private_Phy_Dp83825i_write_mdio_reg(
                DP83825I_PHY_ADDR,
                DP83825I_REG_BMCR,
                DP83825I_BMCR_AUTONEG_ENABLE | DP83825I_BMCR_RESTART_AUTONEG);

            Dp83825i_Status.autonegTimeoutCnt = 0u;
            Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_WAIT_AUTONEG;
            break;
        }

        case LWIP_GETH_PHY_DP83825I_STATE_WAIT_AUTONEG:
        {
            if (lwip_geth_private_Phy_Dp83825i_read_mdio_reg(DP83825I_PHY_ADDR, DP83825I_REG_BMSR, &bmsr) == 0u)
            {
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_ERROR;
                break;
            }

            if ((bmsr & DP83825I_BMSR_AUTONEG_DONE) != 0u)
            {
                Dp83825i_Status.autonegDone = 1u;
                Dp83825i_Status.initDone = 1u;
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_LINK_DOWN;
            }
            else
            {
                Dp83825i_Status.autonegTimeoutCnt++;

                if (Dp83825i_Status.autonegTimeoutCnt >= DP83825I_AUTONEG_TIMEOUT_100MS)
                {
                    Dp83825i_Status.initDone = 1u;
                    Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_LINK_DOWN;
                }
            }

            break;
        }

        case LWIP_GETH_PHY_DP83825I_STATE_LINK_DOWN:
        case LWIP_GETH_PHY_DP83825I_STATE_LINK_UP:
        {
            if (lwip_geth_private_Phy_Dp83825i_read_mdio_reg(DP83825I_PHY_ADDR, DP83825I_REG_BMSR, &bmsr) == 0u)
            {
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_ERROR;
                break;
            }

            if (lwip_geth_private_Phy_Dp83825i_read_mdio_reg(DP83825I_PHY_ADDR, DP83825I_REG_PHYSTS, &physts) == 0u)
            {
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_ERROR;
                break;
            }

            if ((bmsr & DP83825I_BMSR_LINK_STATUS) != 0u)
            {
                Dp83825i_Status.linkUp = 1u;
                Dp83825i_Status.speed100 = ((physts & DP83825I_PHYSTS_SPEED_10) == 0u) ? 1u : 0u;
                Dp83825i_Status.fullDuplex = ((physts & DP83825I_PHYSTS_FULL_DUPLEX) != 0u) ? 1u : 0u;
                Dp83825i_Status.linkUpCnt++;
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_LINK_UP;
            }
            else
            {
                Dp83825i_Status.linkUp = 0u;
                Dp83825i_Status.linkDownCnt++;
                Dp83825i_Status.state = LWIP_GETH_PHY_DP83825I_STATE_LINK_DOWN;
            }

            break;
        }

        case LWIP_GETH_PHY_DP83825I_STATE_ERROR:
        default:
        {
            Dp83825i_Status.linkUp = 0u;
            break;
        }
    }
}

uint32 lwip_geth_private_Phy_Dp83825i_link_status(void)
{
    Ifx_GETH_MAC_PHYIF_CONTROL_STATUS link_status;

    link_status.U = 0u;

    if (Dp83825i_Status.linkUp != 0u)
    {
        link_status.B.LNKSTS = 1u;
        link_status.B.LNKSPEED = (Dp83825i_Status.speed100 != 0u) ? 1u : 0u;
        link_status.B.LNKMOD = (Dp83825i_Status.fullDuplex != 0u) ? 1u : 0u;
    }

    return link_status.U;
}

uint32 lwip_geth_private_Phy_Dp83825i_is_link_up(void)
{
    return Dp83825i_Status.linkUp;
}

const lwip_geth_PhyDp83825i_StatusType *lwip_geth_private_Phy_Dp83825i_getStatus(void)
{
    return &Dp83825i_Status;
}

#endif
