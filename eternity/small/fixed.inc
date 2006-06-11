/*
    Eternity Engine Small Header
    Fixed number support
*/

#if defined _fixed_included
   #endinput
#endif
#define _fixed_included

#define FRACBITS 16
#define FRACUNIT (1 << FRACBITS)
#pragma rational Float

native Fixed:_ffloat(Float:value);
native Fixed:_fmul(Fixed:a, Fixed:b);
native Fixed:_fdiv(Fixed:a, Fixed:b);
native Fixed:_fabs(Fixed:value);
native Fixed:_fsin(Fixed:angle);
native Fixed:_fcos(Fixed:angle);

stock Fixed:fixed(value)
{
   return Fixed:(value << FRACBITS);
}

stock Fixed:_ffract(Fixed:value)
{
   new bool:neg = value < 0;
   new Fixed:result;
   
   result = _fabs(value) & Fixed:(FRACUNIT - 1);
   
   if(neg)
      result = -result;
      
   return result;
}
   
stock Fixed:_ffloor(Fixed:value)
{
   new Fixed:result;
   
   // chop to an integer
   result = _fabs(value) & Fixed:(~(FRACUNIT - 1));
   
   if(value < 0)
   {
      result = -result;
    
      // if it wasn't an integer, subtract one  
      if(result != value)
         result -= Fixed:FRACUNIT;
   }
      
   return result;
}

stock Fixed:_fceil(Fixed:value)
{
   new Fixed:result;
   
   // chop to an integer
   result = _fabs(value) & Fixed:(~(FRACUNIT - 1));
   
   if(value < 0)
      result = -result;
   else if(result != value)
      result += Fixed:FRACUNIT;
      
   return result;
}

stock _ftoi(Fixed:value)
   return (_:value) / FRACUNIT;

native Fixed:operator*(Fixed:a, Fixed:b) = _fmul;
native Fixed:operator/(Fixed:a, Fixed:b) = _fdiv;

stock Fixed:operator=(value)
   return fixed(value);
      
stock Fixed:operator=(Float:value)
   return _ffloat(value);
   
stock Fixed:operator++(Fixed:value)
   return value + Fixed:FRACUNIT;
   
stock Fixed:operator--(Fixed:value)
   return value - Fixed:FRACUNIT;
   
stock Fixed:operator*(Fixed:a, b)
   return Fixed: (_:a * b);            /* "*" is commutative */

stock Fixed:operator/(Fixed:a, b)
   return Fixed: (_:a / b);

stock Fixed:operator/(a, Fixed:b)
   return _fdiv(fixed(a), b);

stock Fixed:operator+(Fixed:a, b)
   return a + fixed(b);                /* "+" is commutative */

stock Fixed:operator-(Fixed:a, b)
   return a - fixed(b);

stock Fixed:operator-(a, Fixed:b)
   return fixed(a) - b;

stock bool:operator>(Fixed:a, b)
   return a > fixed(b);

stock bool:operator>(a, Fixed:b)
   return fixed(a) > b;

stock bool:operator>=(Fixed:a, b)
   return oper1 >= fixed(oper2);

stock bool:operator>=(a, Fixed:b)
   return fixed(a) >= b;

stock bool:operator<(Fixed:oper1, oper2)
   return a < fixed(b);

stock bool:operator<(a, Fixed:b)
   return fixed(a) < b;

stock bool:operator<=(Fixed:a, b)
   return a <= fixed(b);

stock bool:operator<=(a, Fixed:b)
   return fixed(a) <= b;

stock bool:operator==(Fixed:a, b)       /* "==" is commutative */
   return a == fixed(b);

stock bool:operator!=(Fixed:a, b)       /* "!=" is commutative */
   return a != fixed(b);

/* forbidden operations */
forward operator%(Fixed:a, Fixed:b);
forward operator%(Fixed:a, b);
forward operator%(a, Fixed:b);

