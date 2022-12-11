<<<<<<< HEAD
/* SPDX-License-Identifier: GPL-2.0 */
=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#ifndef _ASM_X86_INTEL_FAMILY_H
#define _ASM_X86_INTEL_FAMILY_H

/*
 * "Big Core" Processors (Branded as Core, Xeon, etc...)
 *
<<<<<<< HEAD
 * While adding a new CPUID for a new microarchitecture, add a new
 * group to keep logically sorted out in chronological order. Within
 * that group keep the CPUID for the variants sorted by model number.
 *
 * The defined symbol names have the following form:
 *	INTEL_FAM6{OPTFAMILY}_{MICROARCH}{OPTDIFF}
 * where:
 * OPTFAMILY	Describes the family of CPUs that this belongs to. Default
 *		is assumed to be "_CORE" (and should be omitted). Other values
 *		currently in use are _ATOM and _XEON_PHI
 * MICROARCH	Is the code name for the micro-architecture for this core.
 *		N.B. Not the platform name.
 * OPTDIFF	If needed, a short string to differentiate by market segment.
 *
 *		Common OPTDIFFs:
 *
 *			- regular client parts
 *		_L	- regular mobile parts
 *		_G	- parts with extra graphics on
 *		_X	- regular server parts
 *		_D	- micro server parts
 *
 *		Historical OPTDIFFs:
 *
 *		_EP	- 2 socket server parts
 *		_EX	- 4+ socket server parts
 *
 * The #define line may optionally include a comment including platform names.
=======
 * The "_X" parts are generally the EP and EX Xeons, or the
 * "Extreme" ones, like Broadwell-E.
 *
 * Things ending in "2" are usually because we have no better
 * name for them.  There's no processor called "WESTMERE2".
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
 */

#define INTEL_FAM6_CORE_YONAH		0x0E

#define INTEL_FAM6_CORE2_MEROM		0x0F
#define INTEL_FAM6_CORE2_MEROM_L	0x16
#define INTEL_FAM6_CORE2_PENRYN		0x17
#define INTEL_FAM6_CORE2_DUNNINGTON	0x1D

#define INTEL_FAM6_NEHALEM		0x1E
<<<<<<< HEAD
#define INTEL_FAM6_NEHALEM_G		0x1F /* Auburndale / Havendale */
=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define INTEL_FAM6_NEHALEM_EP		0x1A
#define INTEL_FAM6_NEHALEM_EX		0x2E

#define INTEL_FAM6_WESTMERE		0x25
<<<<<<< HEAD
=======
#define INTEL_FAM6_WESTMERE2		0x1F
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#define INTEL_FAM6_WESTMERE_EP		0x2C
#define INTEL_FAM6_WESTMERE_EX		0x2F

#define INTEL_FAM6_SANDYBRIDGE		0x2A
#define INTEL_FAM6_SANDYBRIDGE_X	0x2D
#define INTEL_FAM6_IVYBRIDGE		0x3A
#define INTEL_FAM6_IVYBRIDGE_X		0x3E

<<<<<<< HEAD
#define INTEL_FAM6_HASWELL		0x3C
#define INTEL_FAM6_HASWELL_X		0x3F
#define INTEL_FAM6_HASWELL_L		0x45
#define INTEL_FAM6_HASWELL_G		0x46

#define INTEL_FAM6_BROADWELL		0x3D
#define INTEL_FAM6_BROADWELL_G		0x47
#define INTEL_FAM6_BROADWELL_X		0x4F
#define INTEL_FAM6_BROADWELL_D		0x56

#define INTEL_FAM6_SKYLAKE_L		0x4E
#define INTEL_FAM6_SKYLAKE		0x5E
#define INTEL_FAM6_SKYLAKE_X		0x55
#define INTEL_FAM6_KABYLAKE_L		0x8E
#define INTEL_FAM6_KABYLAKE		0x9E

#define INTEL_FAM6_CANNONLAKE_L		0x66

#define INTEL_FAM6_ICELAKE_X		0x6A
#define INTEL_FAM6_ICELAKE_D		0x6C
#define INTEL_FAM6_ICELAKE		0x7D
#define INTEL_FAM6_ICELAKE_L		0x7E
#define INTEL_FAM6_ICELAKE_NNPI		0x9D

#define INTEL_FAM6_TIGERLAKE_L		0x8C
#define INTEL_FAM6_TIGERLAKE		0x8D

#define INTEL_FAM6_COMETLAKE		0xA5
#define INTEL_FAM6_COMETLAKE_L		0xA6

/* "Small Core" Processors (Atom) */

#define INTEL_FAM6_ATOM_BONNELL		0x1C /* Diamondville, Pineview */
#define INTEL_FAM6_ATOM_BONNELL_MID	0x26 /* Silverthorne, Lincroft */

#define INTEL_FAM6_ATOM_SALTWELL	0x36 /* Cedarview */
#define INTEL_FAM6_ATOM_SALTWELL_MID	0x27 /* Penwell */
#define INTEL_FAM6_ATOM_SALTWELL_TABLET	0x35 /* Cloverview */

#define INTEL_FAM6_ATOM_SILVERMONT	0x37 /* Bay Trail, Valleyview */
#define INTEL_FAM6_ATOM_SILVERMONT_D	0x4D /* Avaton, Rangely */
#define INTEL_FAM6_ATOM_SILVERMONT_MID	0x4A /* Merriefield */

#define INTEL_FAM6_ATOM_AIRMONT		0x4C /* Cherry Trail, Braswell */
#define INTEL_FAM6_ATOM_AIRMONT_MID	0x5A /* Moorefield */
#define INTEL_FAM6_ATOM_AIRMONT_NP	0x75 /* Lightning Mountain */

#define INTEL_FAM6_ATOM_GOLDMONT	0x5C /* Apollo Lake */
#define INTEL_FAM6_ATOM_GOLDMONT_D	0x5F /* Denverton */

/* Note: the micro-architecture is "Goldmont Plus" */
#define INTEL_FAM6_ATOM_GOLDMONT_PLUS	0x7A /* Gemini Lake */

#define INTEL_FAM6_ATOM_TREMONT_D	0x86 /* Jacobsville */
#define INTEL_FAM6_ATOM_TREMONT		0x96 /* Elkhart Lake */
=======
#define INTEL_FAM6_HASWELL_CORE		0x3C
#define INTEL_FAM6_HASWELL_X		0x3F
#define INTEL_FAM6_HASWELL_ULT		0x45
#define INTEL_FAM6_HASWELL_GT3E		0x46

#define INTEL_FAM6_BROADWELL_CORE	0x3D
#define INTEL_FAM6_BROADWELL_GT3E	0x47
#define INTEL_FAM6_BROADWELL_X		0x4F
#define INTEL_FAM6_BROADWELL_XEON_D	0x56

#define INTEL_FAM6_SKYLAKE_MOBILE	0x4E
#define INTEL_FAM6_SKYLAKE_DESKTOP	0x5E
#define INTEL_FAM6_SKYLAKE_X		0x55
#define INTEL_FAM6_KABYLAKE_MOBILE	0x8E
#define INTEL_FAM6_KABYLAKE_DESKTOP	0x9E

/* "Small Core" Processors (Atom) */

#define INTEL_FAM6_ATOM_PINEVIEW	0x1C
#define INTEL_FAM6_ATOM_LINCROFT	0x26
#define INTEL_FAM6_ATOM_PENWELL		0x27
#define INTEL_FAM6_ATOM_CLOVERVIEW	0x35
#define INTEL_FAM6_ATOM_CEDARVIEW	0x36
#define INTEL_FAM6_ATOM_SILVERMONT1	0x37 /* BayTrail/BYT / Valleyview */
#define INTEL_FAM6_ATOM_SILVERMONT2	0x4D /* Avaton/Rangely */
#define INTEL_FAM6_ATOM_AIRMONT		0x4C /* CherryTrail / Braswell */
#define INTEL_FAM6_ATOM_MERRIFIELD	0x4A /* Tangier */
#define INTEL_FAM6_ATOM_MOOREFIELD	0x5A /* Annidale */
#define INTEL_FAM6_ATOM_GOLDMONT	0x5C
#define INTEL_FAM6_ATOM_DENVERTON	0x5F /* Goldmont Microserver */
#define INTEL_FAM6_ATOM_GEMINI_LAKE	0x7A
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc

/* Xeon Phi */

#define INTEL_FAM6_XEON_PHI_KNL		0x57 /* Knights Landing */
#define INTEL_FAM6_XEON_PHI_KNM		0x85 /* Knights Mill */

<<<<<<< HEAD
/* Useful macros */
#define INTEL_CPU_FAM_ANY(_family, _model, _driver_data)	\
{								\
	.vendor		= X86_VENDOR_INTEL,			\
	.family		= _family,				\
	.model		= _model,				\
	.feature	= X86_FEATURE_ANY,			\
	.driver_data	= (kernel_ulong_t)&_driver_data		\
}

#define INTEL_CPU_FAM6(_model, _driver_data)			\
	INTEL_CPU_FAM_ANY(6, INTEL_FAM6_##_model, _driver_data)

=======
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
#endif /* _ASM_X86_INTEL_FAMILY_H */
