#ifndef __CALC_H
#define __CALC_H

unsigned int v = 0;

unsigned int calc(const char *expr)
{
	const char *p = expr;
	unsigned int result = 0;
	unsigned int term;
	unsigned char op;

	term = 0;
	while (*p >= '0' && *p <= '9') 
	{
		term = term * 10 + (*p - '0');
		p++;
	}
	op = '+';

	while (*p) 
	{
		unsigned char c;
		
		if (*p == '\0') break;

		c = *p++;
		v = 0;
		while (*p >= '0' && *p <= '9') 
		{
			v = v * 10 + (*p - '0');
			p++;
		}
		if (c == '*') 
		{
			term = term * v;
		} 
		else if (c == '/') 
		{
			term = (v == 0) ? 0 : (term / v);
		} 
		else 
		{
			if (op == '+') 
				result += term;
			else 
				result -= term;
			term = v;
			op = c;
		}
	}
	if (op == '+') 
		result += term;
	else
		result -= term;

	return result;
}


#endif