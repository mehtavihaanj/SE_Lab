"""
 * C S 429 Assembly Coding Lab
 *
 * testbench.c - Testbench for all the assembly functions.
 *
 * Copyright (c) 2022, 2023, 2024, 2025.
 * Authors: Prithvi Jamadagni, Kavya Rathod, Anoop Rachakonda
 * All rights reserved.
 * May not be used, modified, or copied without permission.
"""

import sys
import re


def is_xgpr_operand(operand: str):
    return is_xzr(operand) or (operand.startswith("X") and operand[1:].isnumeric())


def is_wreg_operand(operand: str):
    return operand.startswith("W") and (operand[1:] == "ZR" or operand[1:].isnumeric())


def is_xzr(operand: str):
    return operand == "XZR"


def is_sp(operand: str):
    return operand == "SP"


def is_imm(operand: str):
    return not (is_sp(operand) or is_xzr(operand) or is_wreg_operand(operand) or is_xgpr_operand(operand))
    #return operand.isnumeric() or (operand.startswith("#") and operand[1:].isnumeric())

def is_valid_cc(operand: str):
    return operand in [
            "EQ",
            "NE",
            "CS",
            "HS",
            "CC",
            "LO",
            "MI",
            "PL",
            "VS",
            "VC",
            "HI",
            "LS",
            "GE",
            "LT",
            "GT",
            "LE",
            "AL",
        ]

def verify(line: str):
    # now we need to parse the actual instruction
    # chop off any comment if necessary
    line = line.split("//")[0].strip()

    if not line:
        return True, ""
    # ignore labels
    if line.endswith(":"):
        return True, ""
    # ignore comment lines
    if line.startswith("/") or line.startswith("*"):
        return True, ""
    # ignore assembler directives
    if line.startswith("."):
        return True, ""


    opcode = line.split()[0]

    # the opcodes for which there are no variants we have to check
    if opcode in [
        "B",
        "BL",
        "RET",
        "NOP",
        "HLT",
        "ADRP",
    ]:
        return True, ""

    # opcodes for which we just have to do a W reg check
    if opcode in ["LDUR", "STUR", "MOVK", "MOVZ", "MOV", "MVN", "CBZ", "CBNZ"]:
        tokens = re.split(",\\s*|\\s+", line)
        if is_wreg_operand(tokens[1]):
            return False, "using a W register"
        return True, ""

    # b.cc, we don't have to chk variants
    # can probably do better with a regex capture group
    if opcode.startswith("B."):
        return True, ""

    # bcc, we don't have to chk variants
    if (
        opcode.startswith("B")
        and len(opcode) == 3
        and opcode[1:]
        in [
            "EQ",
            "NE",
            "CS",
            "HS",
            "CC",
            "LO",
            "MI",
            "PL",
            "VS",
            "VC",
            "HI",
            "LS",
            "GE",
            "LT",
            "GT",
            "LE",
            "AL",
        ]
    ):
        return True, ""

    # CMP, CMN, and TST
    if opcode in ["CMP", "CMN", "TST"]:
        # should be shifted register version.
        # of the form CMP <Xn>, <Xm>
        # Cannot use the shift!
        tokens = re.split(",\\s*|\\s+", line)
        if len(tokens) > 3:
            return (
                False,
                "using the shift on second operand or using the extended register version",
            )
        elif is_wreg_operand(tokens[1]) or is_wreg_operand(tokens[2]):
            return False, "using W registers"
        # TODO: uncomment the following code for SP2025 (can't add it in F2024 because it is too late)
        elif not is_xgpr_operand(tokens[2]):
            return False, f'using an RI variant of {opcode}'
        else:
            return True, ""

    # ALU_RR opcodes
    if opcode in ["ADDS", "SUBS", "ORR", "EOR", "ANDS"]:
        # Should be the shifted register version.
        # Of the form ADDS <Xd>, <Xn>, <Xm>
        # Cannot use the shift!
        tokens = re.split(",\\s*|\\s+", line)
        if len(tokens) > 4:
            return (
                False,
                "using the shift on second operand or using the extended register version",
            )
        elif is_wreg_operand(tokens[1]) or is_wreg_operand(tokens[2]) or is_wreg_operand(tokens[3]):
            return False, "using W registers"
        elif not is_xgpr_operand(tokens[3]):
            return False, f"using an RI variant of {opcode}"
        else:
            return True, ""

    # ALU_RI opcodes (except UBFM, since it has 2 immediates)
    if opcode in ["ADD", "SUB", "LSL", "LSR", "ASR"]:
        # should be the immediate version
        # Of the form ADD <Xd|SP>, <Xn|SP>, #<imm>
        tokens = re.split(",\\s*|\\s+", line)
        if len(tokens) > 4:
            return (
                False,
                "using the shift on second operand or using the extended register version",
            )
        elif is_wreg_operand(tokens[1]) or is_wreg_operand(tokens[2]):
            return False, "using W registers"
        elif is_xgpr_operand(tokens[3]) or is_sp(tokens[3]) or is_wreg_operand(tokens[3]):
            return False, f"using an RR variant of {opcode}"
        else:
            return True, ""

    # UBFM
    if opcode in ["UBFM"]:
        tokens = re.split(r",\s*|\s+", line)
        if len(tokens) != 5:
            return (
                False,
                'UBFM has wrong number of arguments. Syntax should be "UBFM <Xd>, <Xn>, #<immr>, #<imms>"',
            )
        elif is_wreg_operand(tokens[1]) or is_wreg_operand(tokens[2]):
            return False, "using W registers"
        elif not is_imm(tokens[3]) or not is_imm(tokens[4]):
            return False, 'UBFM must have two immediates. Syntax is: "UBFM <Xd>, <Xn>, #<immr>, #<imms>"'
        else:
            return True, ""

    # Check extra credit

    # the opcodes for which there are no variants we have to check
    if opcode in [
        "BLR",
        "BR",
    ]:
        return True, "extra credit"

    # opcodes for which we just have to do a W reg check
    if opcode in ["CBZ", "CBNZ"]:
        tokens = re.split(",\\s*|\\s+", line)
        if is_wreg_operand(tokens[1]):
            return False, "using a W register"
        return True, "extra credit"

    # CSEL, etc.
    if opcode in ["CSEL", "CSINC", "CSINV", "CSNEG"]:
        tokens = re.split(",\\s*|\\s+", line)
        if len(tokens) != 5:
            return (
                False,
                "incorrect number of parameters for conditional instruction",
            )
        elif is_wreg_operand(tokens[1]) or is_wreg_operand(tokens[2]) or is_wreg_operand(tokens[3]):
            return False, "using W registers"
        elif not is_valid_cc(tokens[4]):
            return False, "invalid condition code"
        return True, ""

    return False, "disallowed opcode"


if len(sys.argv) != 2 and len(sys.argv) != 3:
    print('Usage: "python3 verify.py FILENAME [-ec]"')
    exit(0)

extra_credit = False
if len(sys.argv) == 3:
    extra_credit = sys.argv[2] == "-ec"

fails = 0
with open(sys.argv[1], "r") as infile:
    for i, line in enumerate(infile, 1):
        (valid, reason) = verify(line.strip().upper())
        if not valid:
            fails += 1
            valid_instructions = False
            print(
                    f"File {sys.argv[1]}: Line {i} failed verification. Reason: {reason}. \n\tLine contents: '{line.strip()}'"
            )

        if not extra_credit and valid and "extra credit" in reason:
            print(
                    f"File {sys.argv[1]}: Line {i} uses EC instruction.\n\tLine contents: '{line.strip()}'"
            )

if fails != 0:
    exit(1)
else:
    exit(0)
