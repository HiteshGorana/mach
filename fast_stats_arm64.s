.section __TEXT,__text,regular,pure_instructions
.global _fast_sum
.global _fast_min
.global _fast_max
.global _fast_parse_status
.global _fast_duration_ms
.align 4

// ============================================================
// double fast_sum(double *data, int count)
// Vectorized sum using NEON - processes 2 doubles per iteration
// ============================================================
_fast_sum:
    fmov d0, #0.0
    eor v1.16b, v1.16b, v1.16b
    lsr w2, w1, #1
    cbz w2, .Lsum_tail

.Lsum_loop:
    ld1 {v2.2d}, [x0], #16
    fadd v1.2d, v1.2d, v2.2d
    subs w2, w2, #1
    bne .Lsum_loop
    faddp d0, v1.2d

.Lsum_tail:
    and w2, w1, #1
    cbz w2, .Lsum_done
    ldr d1, [x0]
    fadd d0, d0, d1

.Lsum_done:
    ret

// ============================================================
// double fast_min(double *data, int count)
// Find minimum value using NEON - processes 2 doubles per iteration  
// ============================================================
_fast_min:
    cbz w1, .Lmin_zero
    ldr d0, [x0], #8
    subs w1, w1, #1
    cbz w1, .Lmin_done
    
    mov v1.d[0], v0.d[0]
    mov v1.d[1], v0.d[0]
    
    lsr w2, w1, #1
    cbz w2, .Lmin_tail

.Lmin_loop:
    ld1 {v2.2d}, [x0], #16
    fmin v1.2d, v1.2d, v2.2d
    subs w2, w2, #1
    bne .Lmin_loop
    
    fminp d0, v1.2d

.Lmin_tail:
    and w2, w1, #1
    cbz w2, .Lmin_done
    ldr d1, [x0]
    fmin d0, d0, d1

.Lmin_done:
    ret

.Lmin_zero:
    fmov d0, #0.0
    ret

// ============================================================
// double fast_max(double *data, int count)
// Find maximum value using NEON - processes 2 doubles per iteration
// ============================================================
_fast_max:
    cbz w1, .Lmax_zero
    ldr d0, [x0], #8
    subs w1, w1, #1
    cbz w1, .Lmax_done
    
    mov v1.d[0], v0.d[0]
    mov v1.d[1], v0.d[0]
    
    lsr w2, w1, #1
    cbz w2, .Lmax_tail

.Lmax_loop:
    ld1 {v2.2d}, [x0], #16
    fmax v1.2d, v1.2d, v2.2d
    subs w2, w2, #1
    bne .Lmax_loop
    
    fmaxp d0, v1.2d

.Lmax_tail:
    and w2, w1, #1
    cbz w2, .Lmax_done
    ldr d1, [x0]
    fmax d0, d0, d1

.Lmax_done:
    ret

.Lmax_zero:
    fmov d0, #0.0
    ret

// ============================================================
// int fast_parse_status(char *response)
// Parse HTTP status code from response - optimized for "HTTP/1.1 XXX"
// ============================================================
_fast_parse_status:
    add x0, x0, #9
    mov w1, #0
    mov w2, #3
    
.Lparse_loop:
    ldrb w3, [x0], #1
    sub w3, w3, #48
    cmp w3, #9
    bhi .Lparse_done
    
    mov w4, #10
    mul w1, w1, w4
    add w1, w1, w3
    
    subs w2, w2, #1
    bne .Lparse_loop

.Lparse_done:
    mov w0, w1
    ret

// ============================================================
// double fast_duration_ms(long sec_diff, long nsec_diff)
// Convert timespec diff to milliseconds
// ============================================================
_fast_duration_ms:
    // x0 = seconds difference
    // x1 = nanoseconds difference
    
    // sec * 1000.0
    scvtf d0, x0
    mov w2, #1000
    scvtf d2, w2
    fmul d0, d0, d2
    
    // nsec / 1000000.0
    scvtf d1, x1
    ldr d3, .Lconst_million
    fdiv d1, d1, d3
    
    fadd d0, d0, d1
    ret

.align 3
.Lconst_million:
    .double 1000000.0
