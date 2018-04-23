#define CRC16 0x8005

unsigned short gen_crc16(const unsigned char *data, unsigned short size)
{
  unsigned short out = 0;
  unsigned short crc = 0;
  int bits_read = 0, bit_flag;

  /* Sanity check: */
  if(data == 0)
    return 0;

  while(size > 0)
    {
      bit_flag = out >> 15;

      /* Get next bit: */
      out <<= 1;
      out |= (*data >> bits_read) & 1; // item a) work from the least significant bits

      /* Increment bit counter: */
      bits_read++;
      if(bits_read > 7)
        {
	  bits_read = 0;
	  data++;
	  size--;
        }

      /* Cycle check: */
      if(bit_flag)
	out ^= CRC16;

    }

  // item b) "push out" the last 16 bits
  int i;
  for (i = 0; i < 16; ++i) {
    bit_flag = out >> 15;
    out <<= 1;
    if(bit_flag)
      out ^= CRC16;
  }

  // item c) reverse the bits
  i = 0x8000;
  int j = 0x0001;
  for (; i != 0; i >>=1, j <<= 1) {
    if (i & out) crc |= j;
  }

  return crc;
}
