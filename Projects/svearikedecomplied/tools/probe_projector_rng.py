#!/usr/bin/env python3
"""Run the original RNG instructions in a tiny native i386 ELF, without an emulator.

Only the two pure arithmetic routines are extracted. Their original virtual
addresses are preserved, so their machine instructions need no patching. No
projector startup, Windows API, or script interpreter is executed.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    folder = ROOT/'build/rng-probe'
    folder.mkdir(parents=True, exist_ok=True)
    source = (ROOT/'SveaRike/SVEA95.EXE').read_bytes()
    # .text file offset 0x400, virtual address 0x401000.
    wrapper = source[0x44e770-0x400c00:0x44e7a9-0x400c00]
    generator = source[0x5010d1-0x400c00:0x5010ef-0x400c00]
    assert generator.hex() == 'a130d5510069c0fd43030005c39e2600a330d55100250000ff7fc1e810c3'
    assert wrapper[:4] == bytes.fromhex('8b442404') and wrapper[-3:] == bytes.fromhex('c20400')
    (folder/'wrapper.bin').write_bytes(wrapper)
    (folder/'generator.bin').write_bytes(generator)
    seeds = [0,1,0xffffffff,0x80000000,0x12345678]
    ranges = [1,2,15,20,40,100,0,-1,65535,65536,0x7fffffff,-0x80000000]
    cases = [(seed,bound,1) for seed in seeds for bound in ranges]
    single_count = len(cases)
    # initEventsB, initCountries, fixName(1), initFamilyTree, in script order.
    init_bounds = [15]*6 + [40]*8 + [20,20,2]
    for seed in seeds:
        cases.extend((seed,bound,int(i==0)) for i,bound in enumerate(init_bounds))
    data = '\n'.join(f'.long {seed}, {bound}, {reset}' for seed,bound,reset in cases)
    assembly = f'''.section .random,"ax"
.incbin "wrapper.bin"
.section .rng,"ax"
.incbin "generator.bin"
.section .state,"aw"
.long 0
.section .data,"aw"
cases:
{data}
.section .bss,"aw",@nobits
.lcomm output,{len(cases)*16}
.section .text,"ax"
.global _start
_start:
    mov $cases,%esi
    mov $output,%edi
    mov ${len(cases)},%ebp
next_case:
    cmpl $0,8(%esi)
    je keep_state
    mov (%esi),%eax
    mov %eax,0x51d530
keep_state:
    mov 0x51d530,%eax
    mov %eax,(%edi)
    mov 4(%esi),%eax
    mov %eax,4(%edi)
    push %eax
    mov $0x44e770,%eax
    call *%eax
    mov %eax,8(%edi)
    mov 0x51d530,%eax
    mov %eax,12(%edi)
    add $12,%esi
    add $16,%edi
    dec %ebp
    jne next_case
    mov $4,%eax
    mov $1,%ebx
    mov $output,%ecx
    mov ${len(cases)*16},%edx
    int $0x80
    mov $1,%eax
    xor %ebx,%ebx
    int $0x80
'''
    (folder/'probe.s').write_text(assembly)
    subprocess.run(['as','--32','probe.s','-o','probe.o'],cwd=folder,check=True)
    subprocess.run(['ld','-m','elf_i386','--section-start=.random=0x44e770',
                    '--section-start=.rng=0x5010d1','--section-start=.state=0x51d530',
                    'probe.o','-o','probe'],cwd=folder,check=True)
    run = subprocess.run([str(folder/'probe')],capture_output=True,check=True)
    assert len(run.stdout)==len(cases)*16
    records=[]
    for index,(seed,bound,reset) in enumerate(cases):
        actual_seed,actual_bound,value,state=struct.unpack_from('<IiII',run.stdout,index*16)
        assert actual_bound==bound and (not reset or actual_seed==seed)
        records.append({'seed':actual_seed,'bound':bound,'value':value,'state':state})
    initializations=[]
    for i,seed in enumerate(seeds):
        sequence=records[single_count+i*17:single_count+(i+1)*17]
        initializations.append({'seed':seed,'values':[r['value'] for r in sequence],
                                'state':sequence[-1]['state']})
    report={'source_sha256':hashlib.sha256(source).hexdigest(),
            'wrapper_address':'0x44e770','generator_address':'0x5010d1',
            'execution':'Native Linux i386 ELF, original unmodified arithmetic instructions',
            'vectors':records[:single_count], 'initializations':initializations}
    (ROOT/'analysis/projector-rng-probe.json').write_text(json.dumps(report,indent=2)+'\n')
    header='/* Golden vectors from unmodified SVEA95.EXE instructions, native execution. */\n'
    header+='static const struct { uint32_t seed; int32_t bound; uint32_t value,state; } rng_vectors[] = {\n'
    for r in records[:single_count]:
        header+=f'    {{UINT32_C({r["seed"]}), {r["bound"]}, UINT32_C({r["value"]}), UINT32_C({r["state"]})}},\n'
    header+='};\n'
    header+='static const struct { uint32_t seed, values[17], state; } rng_initializations[] = {\n'
    for r in initializations:
        header+=f'    {{UINT32_C({r["seed"]}), {{{", ".join(map(str,r["values"]))}}}, UINT32_C({r["state"]})}},\n'
    header+='};\n'
    (ROOT/'tests/projector_rng_vectors.h').write_text(header)
    print(f'Captured {single_count} golden vectors and {len(initializations)} initialization sequences from native original instructions.')


if __name__=='__main__':
    main()
