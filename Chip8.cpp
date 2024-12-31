// #include <cstdint>
#include <fstream> // File handling
#include <random> // for random engine and uniform_int_distribution
#include <chrono> // time needed for random
#include <string.h> // for memset

#define START_ADDRESS 0X200
#define FONTSET_SIZE 80 // 16 characters × 5 bytes = 80 bytes.
#define FONTSET_START_ADDRESS 0x50
#define VIDEO_WIDTH  64
#define VIDEO_HEIGHT 32

class Chip8
{
public:
	uint8_t registers[16] = {0};
	uint8_t memory[4096] = {0};
	uint16_t stack[16] = {0};
	uint8_t keypad[16] = {0};
	uint32_t video[64 * 32] = {0};
	uint16_t index = 0;
	uint16_t pc = 0;
	uint8_t sp = 0;
	uint8_t delayTimer = 0;
	uint8_t soundTimer = 0;
	uint16_t opcode;

	/*
	With physical hardware random number in the register could be achieved by,
	reading the value from a noisy disconnected pin or using a dedicated RNG chip, 
	but we’ll just use C++’s built in random facilities.
	*/
	std::default_random_engine randGen; // seed of random byte
	std::uniform_int_distribution<uint8_t> randByte; // uint8 ensures values store in a byte

	uint8_t fontset[FONTSET_SIZE] =
		{
			// Eace character is five byte in size
			0xF0, 0x90, 0x90, 0x90, 0xF0, // 0 
			0x20, 0x60, 0x20, 0x20, 0x70, // 1
			0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
			0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
			0x90, 0x90, 0xF0, 0x10, 0x10, // 4
			0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
			0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
			0xF0, 0x10, 0x20, 0x40, 0x40, // 7
			0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
			0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
			0xF0, 0x90, 0xF0, 0x90, 0x90, // A
			0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
			0xF0, 0x80, 0x80, 0x80, 0xF0, // C
			0xE0, 0x90, 0x90, 0x90, 0xE0, // D
			0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
			0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};

public:
	void Cycle();
	void LoadROM(char const *filename);
	// system instructions
	void OP_00E0(); // CLS
	void OP_00EE(); // RET
	// flow control
	void OP_1nnn(); // Jp to addr
	void OP_2nnn(); // Call subroutine to addr
	void OP_3xkk(); // SE Vx, byte
	void OP_4xkk(); // SNE Vx, byte
	void OP_5xy0(); // SE Vx, Vy
	void OP_6xkk(); // LD Vx, byte
	void OP_7xkk(); // ADD Vx, byte
	void OP_8xy0(); // LD Vx, Vy
	void OP_8xy1(); // OR Vx, Vy
	void OP_8xy2(); // AND Vx, Vy
	void OP_8xy3(); // XOR Vx, Vy
	void OP_8xy4(); // ADD Vx, Vy (if sum is greater than byte than Vf is set 1)
	void OP_8xy5(); // SUB Vx, Vy (if V[x] > V[y] than V[0xF] = norBorrow (set) )
	void OP_8xy6(); // SHR Vx (LSB of Vx = VF)
	void OP_8xy7(); // SUBN Vx, Vy (if V[y] > V[x] than V[0xF] = norBorrow (set) )
	void OP_8xyE(); // SHL Vx (MSB of Vx = VF)
	void OP_9xy0(); // SNE Vx, Vy
	void OP_Annn(); // LD I , addr
	void OP_Bnnn(); // JP V0, addr
	void OP_Cxkk(); // RND Vx, byte
	void OP_Dxyn(); // DRW Vx, Vy, nibble 
	void OP_Ex9E();
	void OP_ExA1();
	void OP_Fx07(); // LD Vx, DT
	void OP_Fx0A(); // wait for keypress , store key in Vx
	void OP_Fx15(); // LD DT, Vx
	void OP_Fx18(); // LD ST, Vx
	void OP_Fx1E(); // ADD I, Vx
	void OP_Fx29();
	void OP_Fx33(); // LD B, Vx
	void OP_Fx55(); // read
	void OP_Fx65(); // write

};

Chip8::Chip8() // Constructor
: pc(START_ADDRESS), 
randGen(std::chrono::system_clock::now().time_since_epoch().count()) // Produces a sequence of random bits.
{
	for (unsigned int i = 0; i < FONTSET_SIZE; ++i)
	{
		// Load fonts into memory
		memory[FONTSET_START_ADDRESS + i] = fontset[i];
	}

	randByte = std::uniform_int_distribution<uint8_t>(0, 255U); // 0-255 matches CHIP-8’s random byte requirement.
}

// Store from memory to the registers
void Chip8::OP_Fx65()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	for (uint8_t i = 0; i <= Vx; ++i)
	{
		registers[i] = memory[index + i];
	}
}

// Store the registers in memory starting from index
void Chip8::OP_Fx55()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	for (uint8_t i = 0; i <= Vx; ++i)
	{
		memory[index + i] = registers[i];
	}
}

/*Store BCD representation of Vx in memory locations I, I+1, and I+2.
The interpreter takes the decimal value of Vx, and places the hundreds digit in memory at location in I, 
the tens digit at location I+1, and the ones digit at location I+2.*/
void Chip8::OP_Fx33()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t value = registers[Vx];

	// Ones-place
	memory[index + 2] = value % 10;
	value /= 10;

	// Tens-place
	memory[index + 1] = value % 10;
	value /= 10;

	// Hundreds-place
	memory[index] = value % 10;
}

// Set Location of sprite
void Chip8::OP_Fx29()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t digit = registers[Vx];

	index = FONTSET_START_ADDRESS + (5 * digit);
}

// Set index
void Chip8::OP_Fx1E()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	index += registers[Vx];
}

// Sound timer
void Chip8::OP_Fx18()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	soundTimer = registers[Vx];
}

// Set delay timer
void Chip8::OP_Fx15()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	delayTimer = registers[Vx];
}

void Chip8::OP_Fx07()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	registers[Vx] = delayTimer;
}

// Skip next instruction if key with the value of Vx is not pressed.
void Chip8::OP_ExA1()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	uint8_t key = registers[Vx];

	if (!keypad[key])
	{
		pc += 2;
	}
}

// Skip next instruction if key with the value of Vx is pressed.
void Chip8::OP_Ex9E()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	uint8_t key = registers[Vx];

	if (keypad[key])
	{
		pc += 2;
	}
}

void Chip8::OP_Dxyn()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;
	uint8_t height = opcode & 0x000Fu;

	// Wrap if going beyond screen boundaries
	uint8_t xPos = registers[Vx] % VIDEO_WIDTH;
	uint8_t yPos = registers[Vy] % VIDEO_HEIGHT;

	registers[0xF] = 0;

	for (unsigned int row = 0; row < height; ++row)
	{
		uint8_t spriteByte = memory[index + row];

		for (unsigned int col = 0; col < 8; ++col)
		{
			uint8_t spritePixel = spriteByte & (0x80u >> col);
			uint32_t* screenPixel = &video[(yPos + row) * VIDEO_WIDTH + (xPos + col)];

			// Sprite pixel is on
			if (spritePixel)
			{
				// Screen pixel also on - collision
				if (*screenPixel == 0xFFFFFFFF)
				{
					registers[0xF] = 1;
				}

				// Effectively XOR with the sprite pixel
				*screenPixel ^= 0xFFFFFFFF;
			}
		}
	}
}

//CLS
void Chip8::OP_00E0()
{
	memset(video, 0, sizeof(video));
}
//RET 
void Chip8::OP_00EE()
{
	--sp;
	pc = stack[sp];
}
// jmp
void Chip8::OP_1nnn()
{
	uint16_t jmp_addr = opcode & 0xfffu;
	pc = jmp_addr;
}
//Call
void Chip8::OP_2nnn()
{
	uint16_t addr = opcode & 0xfffu;
	stack[sp] = pc;
	++sp;
	pc = addr;
}
// Skip if(V[x] == kk)
void Chip8::OP_3xkk()
{
	uint8_t Vx = (opcode & 0x0f00u) >> 8;
	uint8_t kk = opcode & 0x00ffu;

	if(registers[Vx] == kk)
	{
		pc += 2;
	}
}
// skip if(V[x] != kk)
void Chip8::OP_4xkk()
{
	uint8_t Vx = (opcode & 0x0f00u) >> 8u;
	uint8_t kk = opcode & 0x00ffu;

	if(registers[Vx] == kk)
	{
		pc += 2;
	}
}
// skip if(V[x] == V[y])
void Chip8::OP_5xy0()
{
	uint8_t Vx = (opcode & 0x0f00u) >> 8u;
	uint8_t Vy = (opcode & 0x00f0u) >> 4u;

	if(registers[Vx] == registers[Vy])
	{
		pc += 2;
	}
}
// V[x] = kk
void Chip8::OP_6xkk()
{
	uint8_t Vx = (opcode & 0x0f00u) >> 8;
	uint8_t byte = opcode & 0x00ffu;

	registers[Vx] = byte;
}
// V[x] = V[x] + kk
void Chip8::OP_7xkk()
{
	uint8_t Vx = (opcode & 0x0f00u) >> 8;
	uint8_t byte = (opcode & 0x00ffu);

	registers[Vx] += byte;
}
// V[x] = V[y]
void Chip8::OP_8xy0()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	registers[Vx] = registers[Vy];
}
// V[x] |= V[y]
void Chip8::OP_8xy1()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	registers[Vx] = registers[Vx] | registers[Vy];
}
// V[x] &= V[y]
void Chip8::OP_8xy2()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	registers[Vx] = registers[Vx] & registers[Vy];
}
// V[x] ^= V[y]
void Chip8::OP_8xy3()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	registers[Vx] = registers[Vx] ^ registers[Vy];
}
// V[x] += V[y] , and set V[f] if sum > 255U
void Chip8::OP_8xy4()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	uint16_t sum = Vx + Vy;

	if(sum > 0x00ffu)
	{
		registers[0xF] = 1;
	}
	else registers[0xf] = 0;

	registers[Vx] = sum & 0xffu; // Makes sure doesnt overflow
}
// V[x] -= V[y] , and set V[f] if V[x] > V[y]
void Chip8::OP_8xy5()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	if (registers[Vx] > registers[Vy])
	{
		registers[0xF] = 1;
	}
	else
	{
		registers[0xF] = 0;
	}

	registers[Vx] = registers[Vx] - registers[Vy];
}
// V[x] SHR, if LSB of V[x] = 1 than set V[0xF] = 1 else 0
void Chip8::OP_8xy6()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	// Playing smart, avoiding if statement
	registers[0xF] = (registers[Vx] & 0x1u); // 0x1 = LSB

	registers[Vx] = registers[Vx] >> 1;
}
// V[x] = V[y] - V[x] , and set V[f] if V[y] > V[x]
void Chip8::OP_8xy7()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	if (registers[Vy] > registers[Vx])
	{
		registers[0xF] = 1;
	}
	else registers[0xF] = 0;

	registers[Vx] = registers[Vy] - registers[Vx];
}
// V[x] SHL, if MSB of V[x] = 1 than set V[0xF] = 1 else 0
void Chip8::OP_8xyE()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	// Save MSB in VF
	registers[0xF] = (registers[Vx] & 0x80u) >> 7u; // MSB = 0x80

	registers[Vx] = registers[Vx] << 1;
}
// skip if(V[x] != V[y])
void Chip8::OP_9xy0()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t Vy = (opcode & 0x00F0u) >> 4u;

	if (registers[Vx] != registers[Vy])
	{
		pc += 2;
	}
}
// Set indes register to nnn
void Chip8::OP_Annn()
{
	uint16_t address = opcode & 0x0FFFu;
	index = address;
}

void Chip8::OP_Bnnn()
{
	uint16_t address = opcode & 0x0FFFu;
	pc = registers[0] + address;
}

void Chip8::OP_Cxkk()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;
	uint8_t byte = opcode & 0x00FFu;

	registers[Vx] = randByte(randGen) & byte;
}

// Wait for keypress
void Chip8::OP_Fx0A()
{
	uint8_t Vx = (opcode & 0x0F00u) >> 8u;

	if (keypad[0])
	{
		registers[Vx] = 0;
	}
	else if (keypad[1])
	{
		registers[Vx] = 1;
	}
	else if (keypad[2])
	{
		registers[Vx] = 2;
	}
	else if (keypad[3])
	{
		registers[Vx] = 3;
	}
	else if (keypad[4])
	{
		registers[Vx] = 4;
	}
	else if (keypad[5])
	{
		registers[Vx] = 5;
	}
	else if (keypad[6])
	{
		registers[Vx] = 6;
	}
	else if (keypad[7])
	{
		registers[Vx] = 7;
	}
	else if (keypad[8])
	{
		registers[Vx] = 8;
	}
	else if (keypad[9])
	{
		registers[Vx] = 9;
	}
	else if (keypad[10])
	{
		registers[Vx] = 10;
	}
	else if (keypad[11])
	{
		registers[Vx] = 11;
	}
	else if (keypad[12])
	{
		registers[Vx] = 12;
	}
	else if (keypad[13])
	{
		registers[Vx] = 13;
	}
	else if (keypad[14])
	{
		registers[Vx] = 14;
	}
	else if (keypad[15])
	{
		registers[Vx] = 15;
	}
	else
	{
		pc -= 2;
	}
}


// void Chip8::Cycle()
// {
// 	// Fetch
// 	opcode = (memory[pc] << 8u) | memory[pc + 1]; // shifting 8 bits left to msb

// 	// Increment the PC before we execute anything
// 	pc += 2;

// 	// Decode and Execute
// 	((this->*(table[(opcode & 0xF000u) >> 12u]))();

// 	// Decrement the delay timer if it's been set
// 	if (delayTimer > 0)
// 	{
// 		--delayTimer;
// 	}

// 	// Decrement the sound timer if it's been set
// 	if (soundTimer > 0)
// 	{
// 		--soundTimer;
// 	}
// }

void Chip8::LoadROM(char const *filename)
{
	// Open the file as a stream of binary and move the file pointer to the end
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	if (file.is_open())
	{
		// Get size of file and allocate a buffer to hold the contents
		std::streampos size = file.tellg();
		char *buffer = new char[size];

		// Go back to the beginning of the file and fill the buffer
		file.seekg(0, std::ios::beg);
		file.read(buffer, size);
		file.close();

		// Load the ROM contents into the Chip8's memory, starting at 0x200
		for (long i = 0; i < size; ++i)
		{
			memory[START_ADDRESS + i] = buffer[i];
		}

		// Free the buffer
		delete[] buffer;
	}
}