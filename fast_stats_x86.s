# x86_64 SSE/AVX Assembly for Blitz-C
# Works on Linux x86_64 and Windows x64

.intel_syntax noprefix
.text

.global fast_sum
.global fast_min
.global fast_max
.global fast_parse_status
.global fast_duration_ms

# ============================================================
# double fast_sum(double *data, int count)
# Vectorized sum using SSE2 - processes 2 doubles per iteration
# System V AMD64 ABI: rdi = data, esi = count
# ============================================================
fast_sum:
    xorpd xmm0, xmm0            # result = 0.0
    xorpd xmm1, xmm1            # accumulator
    
    mov eax, esi
    shr eax, 1                  # count / 2
    test eax, eax
    jz .Lsum_tail_x86

.Lsum_loop_x86:
    movupd xmm2, [rdi]          # Load 2 doubles
    addpd xmm1, xmm2            # Accumulate
    add rdi, 16
    dec eax
    jnz .Lsum_loop_x86
    
    # Horizontal add: xmm0 = xmm1[0] + xmm1[1]
    movapd xmm0, xmm1
    unpckhpd xmm1, xmm1
    addsd xmm0, xmm1

.Lsum_tail_x86:
    test esi, 1
    jz .Lsum_done_x86
    addsd xmm0, [rdi]

.Lsum_done_x86:
    ret

# ============================================================
# double fast_min(double *data, int count)
# Find minimum using SSE2
# ============================================================
fast_min:
    test esi, esi
    jz .Lmin_zero_x86
    
    movsd xmm0, [rdi]           # xmm0 = first element
    add rdi, 8
    dec esi
    jz .Lmin_done_x86
    
    movapd xmm1, xmm0           # Initialize both lanes
    unpcklpd xmm1, xmm0
    
    mov eax, esi
    shr eax, 1
    test eax, eax
    jz .Lmin_tail_x86

.Lmin_loop_x86:
    movupd xmm2, [rdi]
    minpd xmm1, xmm2
    add rdi, 16
    dec eax
    jnz .Lmin_loop_x86
    
    movapd xmm0, xmm1
    unpckhpd xmm1, xmm1
    minsd xmm0, xmm1

.Lmin_tail_x86:
    test esi, 1
    jz .Lmin_done_x86
    minsd xmm0, [rdi]

.Lmin_done_x86:
    ret

.Lmin_zero_x86:
    xorpd xmm0, xmm0
    ret

# ============================================================
# double fast_max(double *data, int count)
# Find maximum using SSE2
# ============================================================
fast_max:
    test esi, esi
    jz .Lmax_zero_x86
    
    movsd xmm0, [rdi]
    add rdi, 8
    dec esi
    jz .Lmax_done_x86
    
    movapd xmm1, xmm0
    unpcklpd xmm1, xmm0
    
    mov eax, esi
    shr eax, 1
    test eax, eax
    jz .Lmax_tail_x86

.Lmax_loop_x86:
    movupd xmm2, [rdi]
    maxpd xmm1, xmm2
    add rdi, 16
    dec eax
    jnz .Lmax_loop_x86
    
    movapd xmm0, xmm1
    unpckhpd xmm1, xmm1
    maxsd xmm0, xmm1

.Lmax_tail_x86:
    test esi, 1
    jz .Lmax_done_x86
    maxsd xmm0, [rdi]

.Lmax_done_x86:
    ret

.Lmax_zero_x86:
    xorpd xmm0, xmm0
    ret

# ============================================================
# int fast_parse_status(char *response)
# Parse HTTP status code - expects "HTTP/1.x YYY"
# ============================================================
fast_parse_status:
    add rdi, 9                  # Skip "HTTP/1.x "
    xor eax, eax                # result = 0
    mov ecx, 3                  # 3 digits

.Lparse_loop_x86:
    movzx edx, byte ptr [rdi]
    inc rdi
    sub edx, 48                 # '0' = 48
    cmp edx, 9
    ja .Lparse_done_x86
    
    imul eax, eax, 10
    add eax, edx
    dec ecx
    jnz .Lparse_loop_x86

.Lparse_done_x86:
    ret

# ============================================================
# double fast_duration_ms(long sec_diff, long nsec_diff)
# Convert timespec diff to milliseconds
# System V AMD64 ABI: rdi = sec, rsi = nsec
# ============================================================
fast_duration_ms:
    cvtsi2sd xmm0, rdi          # sec -> double
    mov rax, 1000
    cvtsi2sd xmm2, rax
    mulsd xmm0, xmm2            # sec * 1000
    
    cvtsi2sd xmm1, rsi          # nsec -> double
    mov rax, 1000000
    cvtsi2sd xmm3, rax
    divsd xmm1, xmm3            # nsec / 1000000
    
    addsd xmm0, xmm1
    ret
