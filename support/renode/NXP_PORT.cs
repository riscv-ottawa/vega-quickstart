// NXP_PORT.cs - NXP PORT pin-mux + pin-interrupt controller model for Renode
//
// Models the subset of the RV32M1 PORT block that GPIO pin interrupts
// need, so a button on PORTA can be exercised in simulation:
//
//   0x00..0x7C  PCR[0..31]  Pin Control Registers; the IRQC field is
//                           bits [19:16] (PORT_PCR_IRQC).
//   0xA0        ISFR        Interrupt Status Flag Register, write-1-to-clear.
//
// Any other offset (GPCLR/GPCHR, etc.) behaves as a plain register file
// so driver writes do not bus-fault, matching the Python stub this
// replaces.
//
// Pin input levels arrive through IGPIOReceiver.OnGPIO. Connect a button
// in the .repl, or drive it from the monitor:
//
//   sysbus.porta OnGPIO 0 false   # press   (idle is high; falling edge)
//   sysbus.porta OnGPIO 0 true    # release (rising edge)
//
// On an edge matching a pin's IRQC config the corresponding ISFR bit is
// set and the IRQ line is raised. The line follows ISFR, so clearing
// ISFR (what GPIO_ClearPinsInterruptFlags does) lowers it again. The
// EVENT_UNIT is stubbed in this platform, so PORTA's IRQ wires straight
// to the core at cpu@18 (PORTA_IRQn), the same way the LPTMR reaches the
// core through INTMUX without a modelled EVENT_UNIT.

using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.GPIOPort
{
    // The SDK writes some PCR fields with 16-bit accesses (port_pin_config_t
    // is 16 bits wide), so let the bus synthesize byte/word accesses from the
    // DoubleWord implementation instead of dropping them.
    [AllowedTranslations(AllowedTranslation.ByteToDoubleWord | AllowedTranslation.WordToDoubleWord |
                         AllowedTranslation.DoubleWordToByte | AllowedTranslation.DoubleWordToWord)]
    public class NXP_PORT : IDoubleWordPeripheral, IKnownSize, IGPIOReceiver
    {
        public NXP_PORT(IMachine machine)
        {
            IRQ = new GPIO();
            regs = new uint[Size / 4];
            level = new bool[NumberOfPins];
            Reset();
        }

        public uint ReadDoubleWord(long offset)
        {
            return regs[offset >> 2];
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            if(offset == ISFR_Offset)
            {
                // Interrupt Status Flag Register: write 1 to clear.
                regs[offset >> 2] &= ~value;
                UpdateIRQ();
                return;
            }
            regs[offset >> 2] = value;
        }

        public void OnGPIO(int number, bool value)
        {
            if(number < 0 || number >= NumberOfPins)
            {
                this.Log(LogLevel.Warning, "Ignoring edge on out-of-range pin {0}", number);
                return;
            }

            var previous = level[number];
            level[number] = value;

            var irqc = (regs[number] & IRQC_Mask) >> IRQC_Shift;
            bool fire;
            switch(irqc)
            {
                case 0x8: fire = !value;                break; // logic 0 (level)
                case 0x9: fire = !previous && value;    break; // rising edge
                case 0xA: fire = previous && !value;    break; // falling edge
                case 0xB: fire = previous != value;     break; // either edge
                case 0xC: fire = value;                 break; // logic 1 (level)
                default:  fire = false;                 break; // 0x0: disabled
            }

            if(fire)
            {
                regs[ISFR_Offset >> 2] |= (1u << number);
                UpdateIRQ();
            }
        }

        public void Reset()
        {
            for(int i = 0; i < regs.Length; i++)
            {
                regs[i] = 0;
            }
            // Inputs idle high, modelling the pull-ups that buttons use,
            // so the first "press" (drive low) is a falling edge.
            for(int i = 0; i < NumberOfPins; i++)
            {
                level[i] = true;
            }
            IRQ.Unset();
        }

        public long Size => 0xD0;
        public GPIO IRQ { get; private set; }

        private void UpdateIRQ()
        {
            IRQ.Set(regs[ISFR_Offset >> 2] != 0);
        }

        private readonly uint[] regs;
        private readonly bool[] level;

        private const int NumberOfPins = 32;
        private const long ISFR_Offset = 0xA0;
        private const uint IRQC_Mask = 0xF0000u;
        private const int IRQC_Shift = 16;
    }
}
