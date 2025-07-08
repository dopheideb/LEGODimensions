#define SYSTEM_CLOCK1_ENABLE_CAPT
#define USB_ENABLE_SERIAL
#define USB_SERIAL_SIZE 128
#define CALL_loop
#include <amat.hh>

// D16: B2	input A
// D15: B1	input B
// D10: B6	output

#define PIN 6
#define A 2
#define B 4

static void set_pin()
{
	PORTB |= 1 << PIN;
}

static void clear_pin()
{
	PORTB &= ~(1 << PIN);
}

void setup()
{
	DDRB = 1 << PIN;
	PORTB = A | B;	// Set pull-ups on inputs.
	sei();
}

static volatile bool running = false;
static uint8_t old_state = 0;

void loop()
{
	if (!running)
		return;
	
	// Command received; start glitching.
	uint8_t state = PINB & (A | B);
	if (state == old_state)
		return;
	switch (state) {
	case A | B:
		if (old_state == A)
			return;
		// lpc has booted, waiting for pin to be high.
		Usb::serial_tx_print("pulsing");
		set_pin();
		break;
	case A:
		if (old_state == 0)
			return;
		// lpc has seen high pin, waiting for pin to be low.
		Usb::serial_tx_print("done pulsing");
		clear_pin();
		break;
	case 0:
		if (old_state == B)
			return;
		// lpc is looping.
		// TODO: Glitch it.
		Usb::serial_tx_print("glitching");
		while (true) {
			set_pin();
			Counter::busy_wait(100);
			clear_pin();
			Counter::busy_wait(100);
			if ((PINB & (A | B)) == B)
				break;
		}
		break;
	case B:
		if (old_state == (A | B))
			return;
		// lpc has glitched.
		Usb::serial_tx_print("success!");
		while (true) {}
	default:
		// wtf?
		Usb::serial_tx_print("wtf?");
		break;
	}
	old_state = state;
}

void usb_serial_rx(uint8_t c, uint8_t len)
{
	if (c != '\n' && c != '\r') {
		Usb::serial_tx_print("no newline");
		return;
	}
	Usb::serial_rx_pop(len);
	Usb::serial_tx_print("Glitching");
	running = true;
}
