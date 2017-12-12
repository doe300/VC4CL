/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_BITFIELD
#define VC4CL_BITFIELD

#include <stdint.h>

namespace  vc4cl
{
	template<typename UnderlyingType>
	struct Bitfield
	{
		static constexpr uint8_t MASK_Bit { 0x1 };
		static constexpr uint8_t MASK_Tuple { 0x3 };
		static constexpr uint8_t MASK_Triple { 0x7 };
		static constexpr uint8_t MASK_Quadruple { 0xF };
		static constexpr uint8_t MASK_Quintuple { 0x1F };
		static constexpr uint8_t MASK_Sextuple { 0x3F };
		static constexpr uint8_t MASK_Septuple { 0x7F };
		static constexpr uint8_t MASK_Byte { 0xFF };
		static constexpr uint16_t MASK_Nonuple = { 0x1FF };
		static constexpr uint16_t MASK_Decuple = { 0x3FF };
		static constexpr uint16_t MASK_Undecuple = { 0x7FF };
		static constexpr uint16_t MASK_Duodecuple = { 0xFFF };
		static constexpr uint16_t MASK_Tredecuple = { 0x1FFF };
		static constexpr uint16_t MASK_Short { 0xFFFF };
		static constexpr uint16_t MASK_SignedShort { 0xFFFF };
		static constexpr uint32_t MASK_Vigintuple { 0xFFFFF };
		static constexpr uint32_t MASK_Quattuorvigintuple { 0xFFFFFF };
		static constexpr uint32_t MASK_Duovigintuple { 0x3FFFFF };
		static constexpr uint32_t MASK_Int { 0xFFFFFFFF };
		static constexpr uint32_t MASK_SignedInt { 0xFFFFFFFF };

		constexpr Bitfield(UnderlyingType val = 0) : value(val)
		{

		}

		template<typename T>
		inline void setEntry(T val, uint8_t pos, UnderlyingType mask)
		{
			/* since for some fields, the "default" or "not set" value has not the bit-mask 0..0, we need to clear them first */
			value &= ~(mask << pos);
			value |= (mask & static_cast<UnderlyingType>(val)) << pos;
		}

		template<typename T>
		constexpr inline T getEntry(uint8_t pos, UnderlyingType mask) const
		{
			return static_cast<T>(mask & getValue(pos));
		}

#define BITFIELD_ENTRY(name, Type, pos, length) \
	inline Type get##name() const { \
		return getEntry<Type>(pos, MASK_##length); \
	} \
	inline void set##name(Type val) { \
		setEntry<Type>(val, pos, MASK_##length); \
	}

		UnderlyingType value;

		constexpr inline UnderlyingType getValue(uint8_t startPos) const
		{
			return value >> startPos;
		}
	};


}  // namespace  vc4cl



#endif /* VC4CL_BITFIELD */
