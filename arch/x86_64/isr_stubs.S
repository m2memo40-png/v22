.section .text
.extern isr_dispatch
.global isr_stub_table
.align 16
.global isr0
.type isr0,@function
isr0:
    pushq $0
    pushq $0
    jmp isr_common
.size isr0, .-isr0
.global isr1
.type isr1,@function
isr1:
    pushq $0
    pushq $1
    jmp isr_common
.size isr1, .-isr1
.global isr2
.type isr2,@function
isr2:
    pushq $0
    pushq $2
    jmp isr_common
.size isr2, .-isr2
.global isr3
.type isr3,@function
isr3:
    pushq $0
    pushq $3
    jmp isr_common
.size isr3, .-isr3
.global isr4
.type isr4,@function
isr4:
    pushq $0
    pushq $4
    jmp isr_common
.size isr4, .-isr4
.global isr5
.type isr5,@function
isr5:
    pushq $0
    pushq $5
    jmp isr_common
.size isr5, .-isr5
.global isr6
.type isr6,@function
isr6:
    pushq $0
    pushq $6
    jmp isr_common
.size isr6, .-isr6
.global isr7
.type isr7,@function
isr7:
    pushq $0
    pushq $7
    jmp isr_common
.size isr7, .-isr7
.global isr8
.type isr8,@function
isr8:
    pushq $8
    jmp isr_common
.size isr8, .-isr8
.global isr9
.type isr9,@function
isr9:
    pushq $0
    pushq $9
    jmp isr_common
.size isr9, .-isr9
.global isr10
.type isr10,@function
isr10:
    pushq $10
    jmp isr_common
.size isr10, .-isr10
.global isr11
.type isr11,@function
isr11:
    pushq $11
    jmp isr_common
.size isr11, .-isr11
.global isr12
.type isr12,@function
isr12:
    pushq $12
    jmp isr_common
.size isr12, .-isr12
.global isr13
.type isr13,@function
isr13:
    pushq $13
    jmp isr_common
.size isr13, .-isr13
.global isr14
.type isr14,@function
isr14:
    pushq $14
    jmp isr_common
.size isr14, .-isr14
.global isr15
.type isr15,@function
isr15:
    pushq $0
    pushq $15
    jmp isr_common
.size isr15, .-isr15
.global isr16
.type isr16,@function
isr16:
    pushq $0
    pushq $16
    jmp isr_common
.size isr16, .-isr16
.global isr17
.type isr17,@function
isr17:
    pushq $17
    jmp isr_common
.size isr17, .-isr17
.global isr18
.type isr18,@function
isr18:
    pushq $0
    pushq $18
    jmp isr_common
.size isr18, .-isr18
.global isr19
.type isr19,@function
isr19:
    pushq $0
    pushq $19
    jmp isr_common
.size isr19, .-isr19
.global isr20
.type isr20,@function
isr20:
    pushq $0
    pushq $20
    jmp isr_common
.size isr20, .-isr20
.global isr21
.type isr21,@function
isr21:
    pushq $21
    jmp isr_common
.size isr21, .-isr21
.global isr22
.type isr22,@function
isr22:
    pushq $0
    pushq $22
    jmp isr_common
.size isr22, .-isr22
.global isr23
.type isr23,@function
isr23:
    pushq $0
    pushq $23
    jmp isr_common
.size isr23, .-isr23
.global isr24
.type isr24,@function
isr24:
    pushq $0
    pushq $24
    jmp isr_common
.size isr24, .-isr24
.global isr25
.type isr25,@function
isr25:
    pushq $0
    pushq $25
    jmp isr_common
.size isr25, .-isr25
.global isr26
.type isr26,@function
isr26:
    pushq $0
    pushq $26
    jmp isr_common
.size isr26, .-isr26
.global isr27
.type isr27,@function
isr27:
    pushq $0
    pushq $27
    jmp isr_common
.size isr27, .-isr27
.global isr28
.type isr28,@function
isr28:
    pushq $0
    pushq $28
    jmp isr_common
.size isr28, .-isr28
.global isr29
.type isr29,@function
isr29:
    pushq $29
    jmp isr_common
.size isr29, .-isr29
.global isr30
.type isr30,@function
isr30:
    pushq $30
    jmp isr_common
.size isr30, .-isr30
.global isr31
.type isr31,@function
isr31:
    pushq $0
    pushq $31
    jmp isr_common
.size isr31, .-isr31
.global isr32
.type isr32,@function
isr32:
    pushq $0
    pushq $32
    jmp isr_common
.size isr32, .-isr32
.global isr33
.type isr33,@function
isr33:
    pushq $0
    pushq $33
    jmp isr_common
.size isr33, .-isr33
.global isr34
.type isr34,@function
isr34:
    pushq $0
    pushq $34
    jmp isr_common
.size isr34, .-isr34
.global isr35
.type isr35,@function
isr35:
    pushq $0
    pushq $35
    jmp isr_common
.size isr35, .-isr35
.global isr36
.type isr36,@function
isr36:
    pushq $0
    pushq $36
    jmp isr_common
.size isr36, .-isr36
.global isr37
.type isr37,@function
isr37:
    pushq $0
    pushq $37
    jmp isr_common
.size isr37, .-isr37
.global isr38
.type isr38,@function
isr38:
    pushq $0
    pushq $38
    jmp isr_common
.size isr38, .-isr38
.global isr39
.type isr39,@function
isr39:
    pushq $0
    pushq $39
    jmp isr_common
.size isr39, .-isr39
.global isr40
.type isr40,@function
isr40:
    pushq $0
    pushq $40
    jmp isr_common
.size isr40, .-isr40
.global isr41
.type isr41,@function
isr41:
    pushq $0
    pushq $41
    jmp isr_common
.size isr41, .-isr41
.global isr42
.type isr42,@function
isr42:
    pushq $0
    pushq $42
    jmp isr_common
.size isr42, .-isr42
.global isr43
.type isr43,@function
isr43:
    pushq $0
    pushq $43
    jmp isr_common
.size isr43, .-isr43
.global isr44
.type isr44,@function
isr44:
    pushq $0
    pushq $44
    jmp isr_common
.size isr44, .-isr44
.global isr45
.type isr45,@function
isr45:
    pushq $0
    pushq $45
    jmp isr_common
.size isr45, .-isr45
.global isr46
.type isr46,@function
isr46:
    pushq $0
    pushq $46
    jmp isr_common
.size isr46, .-isr46
.global isr47
.type isr47,@function
isr47:
    pushq $0
    pushq $47
    jmp isr_common
.size isr47, .-isr47
.global isr48
.type isr48,@function
isr48:
    pushq $0
    pushq $48
    jmp isr_common
.size isr48, .-isr48
.global isr49
.type isr49,@function
isr49:
    pushq $0
    pushq $49
    jmp isr_common
.size isr49, .-isr49
.global isr50
.type isr50,@function
isr50:
    pushq $0
    pushq $50
    jmp isr_common
.size isr50, .-isr50
.global isr51
.type isr51,@function
isr51:
    pushq $0
    pushq $51
    jmp isr_common
.size isr51, .-isr51
.global isr52
.type isr52,@function
isr52:
    pushq $0
    pushq $52
    jmp isr_common
.size isr52, .-isr52
.global isr53
.type isr53,@function
isr53:
    pushq $0
    pushq $53
    jmp isr_common
.size isr53, .-isr53
.global isr54
.type isr54,@function
isr54:
    pushq $0
    pushq $54
    jmp isr_common
.size isr54, .-isr54
.global isr55
.type isr55,@function
isr55:
    pushq $0
    pushq $55
    jmp isr_common
.size isr55, .-isr55
.global isr56
.type isr56,@function
isr56:
    pushq $0
    pushq $56
    jmp isr_common
.size isr56, .-isr56
.global isr57
.type isr57,@function
isr57:
    pushq $0
    pushq $57
    jmp isr_common
.size isr57, .-isr57
.global isr58
.type isr58,@function
isr58:
    pushq $0
    pushq $58
    jmp isr_common
.size isr58, .-isr58
.global isr59
.type isr59,@function
isr59:
    pushq $0
    pushq $59
    jmp isr_common
.size isr59, .-isr59
.global isr60
.type isr60,@function
isr60:
    pushq $0
    pushq $60
    jmp isr_common
.size isr60, .-isr60
.global isr61
.type isr61,@function
isr61:
    pushq $0
    pushq $61
    jmp isr_common
.size isr61, .-isr61
.global isr62
.type isr62,@function
isr62:
    pushq $0
    pushq $62
    jmp isr_common
.size isr62, .-isr62
.global isr63
.type isr63,@function
isr63:
    pushq $0
    pushq $63
    jmp isr_common
.size isr63, .-isr63
.global isr64
.type isr64,@function
isr64:
    pushq $0
    pushq $64
    jmp isr_common
.size isr64, .-isr64
.global isr65
.type isr65,@function
isr65:
    pushq $0
    pushq $65
    jmp isr_common
.size isr65, .-isr65
.global isr66
.type isr66,@function
isr66:
    pushq $0
    pushq $66
    jmp isr_common
.size isr66, .-isr66
.global isr67
.type isr67,@function
isr67:
    pushq $0
    pushq $67
    jmp isr_common
.size isr67, .-isr67
.global isr68
.type isr68,@function
isr68:
    pushq $0
    pushq $68
    jmp isr_common
.size isr68, .-isr68
.global isr69
.type isr69,@function
isr69:
    pushq $0
    pushq $69
    jmp isr_common
.size isr69, .-isr69
.global isr70
.type isr70,@function
isr70:
    pushq $0
    pushq $70
    jmp isr_common
.size isr70, .-isr70
.global isr71
.type isr71,@function
isr71:
    pushq $0
    pushq $71
    jmp isr_common
.size isr71, .-isr71
.global isr72
.type isr72,@function
isr72:
    pushq $0
    pushq $72
    jmp isr_common
.size isr72, .-isr72
.global isr73
.type isr73,@function
isr73:
    pushq $0
    pushq $73
    jmp isr_common
.size isr73, .-isr73
.global isr74
.type isr74,@function
isr74:
    pushq $0
    pushq $74
    jmp isr_common
.size isr74, .-isr74
.global isr75
.type isr75,@function
isr75:
    pushq $0
    pushq $75
    jmp isr_common
.size isr75, .-isr75
.global isr76
.type isr76,@function
isr76:
    pushq $0
    pushq $76
    jmp isr_common
.size isr76, .-isr76
.global isr77
.type isr77,@function
isr77:
    pushq $0
    pushq $77
    jmp isr_common
.size isr77, .-isr77
.global isr78
.type isr78,@function
isr78:
    pushq $0
    pushq $78
    jmp isr_common
.size isr78, .-isr78
.global isr79
.type isr79,@function
isr79:
    pushq $0
    pushq $79
    jmp isr_common
.size isr79, .-isr79
.global isr80
.type isr80,@function
isr80:
    pushq $0
    pushq $80
    jmp isr_common
.size isr80, .-isr80
.global isr81
.type isr81,@function
isr81:
    pushq $0
    pushq $81
    jmp isr_common
.size isr81, .-isr81
.global isr82
.type isr82,@function
isr82:
    pushq $0
    pushq $82
    jmp isr_common
.size isr82, .-isr82
.global isr83
.type isr83,@function
isr83:
    pushq $0
    pushq $83
    jmp isr_common
.size isr83, .-isr83
.global isr84
.type isr84,@function
isr84:
    pushq $0
    pushq $84
    jmp isr_common
.size isr84, .-isr84
.global isr85
.type isr85,@function
isr85:
    pushq $0
    pushq $85
    jmp isr_common
.size isr85, .-isr85
.global isr86
.type isr86,@function
isr86:
    pushq $0
    pushq $86
    jmp isr_common
.size isr86, .-isr86
.global isr87
.type isr87,@function
isr87:
    pushq $0
    pushq $87
    jmp isr_common
.size isr87, .-isr87
.global isr88
.type isr88,@function
isr88:
    pushq $0
    pushq $88
    jmp isr_common
.size isr88, .-isr88
.global isr89
.type isr89,@function
isr89:
    pushq $0
    pushq $89
    jmp isr_common
.size isr89, .-isr89
.global isr90
.type isr90,@function
isr90:
    pushq $0
    pushq $90
    jmp isr_common
.size isr90, .-isr90
.global isr91
.type isr91,@function
isr91:
    pushq $0
    pushq $91
    jmp isr_common
.size isr91, .-isr91
.global isr92
.type isr92,@function
isr92:
    pushq $0
    pushq $92
    jmp isr_common
.size isr92, .-isr92
.global isr93
.type isr93,@function
isr93:
    pushq $0
    pushq $93
    jmp isr_common
.size isr93, .-isr93
.global isr94
.type isr94,@function
isr94:
    pushq $0
    pushq $94
    jmp isr_common
.size isr94, .-isr94
.global isr95
.type isr95,@function
isr95:
    pushq $0
    pushq $95
    jmp isr_common
.size isr95, .-isr95
.global isr96
.type isr96,@function
isr96:
    pushq $0
    pushq $96
    jmp isr_common
.size isr96, .-isr96
.global isr97
.type isr97,@function
isr97:
    pushq $0
    pushq $97
    jmp isr_common
.size isr97, .-isr97
.global isr98
.type isr98,@function
isr98:
    pushq $0
    pushq $98
    jmp isr_common
.size isr98, .-isr98
.global isr99
.type isr99,@function
isr99:
    pushq $0
    pushq $99
    jmp isr_common
.size isr99, .-isr99
.global isr100
.type isr100,@function
isr100:
    pushq $0
    pushq $100
    jmp isr_common
.size isr100, .-isr100
.global isr101
.type isr101,@function
isr101:
    pushq $0
    pushq $101
    jmp isr_common
.size isr101, .-isr101
.global isr102
.type isr102,@function
isr102:
    pushq $0
    pushq $102
    jmp isr_common
.size isr102, .-isr102
.global isr103
.type isr103,@function
isr103:
    pushq $0
    pushq $103
    jmp isr_common
.size isr103, .-isr103
.global isr104
.type isr104,@function
isr104:
    pushq $0
    pushq $104
    jmp isr_common
.size isr104, .-isr104
.global isr105
.type isr105,@function
isr105:
    pushq $0
    pushq $105
    jmp isr_common
.size isr105, .-isr105
.global isr106
.type isr106,@function
isr106:
    pushq $0
    pushq $106
    jmp isr_common
.size isr106, .-isr106
.global isr107
.type isr107,@function
isr107:
    pushq $0
    pushq $107
    jmp isr_common
.size isr107, .-isr107
.global isr108
.type isr108,@function
isr108:
    pushq $0
    pushq $108
    jmp isr_common
.size isr108, .-isr108
.global isr109
.type isr109,@function
isr109:
    pushq $0
    pushq $109
    jmp isr_common
.size isr109, .-isr109
.global isr110
.type isr110,@function
isr110:
    pushq $0
    pushq $110
    jmp isr_common
.size isr110, .-isr110
.global isr111
.type isr111,@function
isr111:
    pushq $0
    pushq $111
    jmp isr_common
.size isr111, .-isr111
.global isr112
.type isr112,@function
isr112:
    pushq $0
    pushq $112
    jmp isr_common
.size isr112, .-isr112
.global isr113
.type isr113,@function
isr113:
    pushq $0
    pushq $113
    jmp isr_common
.size isr113, .-isr113
.global isr114
.type isr114,@function
isr114:
    pushq $0
    pushq $114
    jmp isr_common
.size isr114, .-isr114
.global isr115
.type isr115,@function
isr115:
    pushq $0
    pushq $115
    jmp isr_common
.size isr115, .-isr115
.global isr116
.type isr116,@function
isr116:
    pushq $0
    pushq $116
    jmp isr_common
.size isr116, .-isr116
.global isr117
.type isr117,@function
isr117:
    pushq $0
    pushq $117
    jmp isr_common
.size isr117, .-isr117
.global isr118
.type isr118,@function
isr118:
    pushq $0
    pushq $118
    jmp isr_common
.size isr118, .-isr118
.global isr119
.type isr119,@function
isr119:
    pushq $0
    pushq $119
    jmp isr_common
.size isr119, .-isr119
.global isr120
.type isr120,@function
isr120:
    pushq $0
    pushq $120
    jmp isr_common
.size isr120, .-isr120
.global isr121
.type isr121,@function
isr121:
    pushq $0
    pushq $121
    jmp isr_common
.size isr121, .-isr121
.global isr122
.type isr122,@function
isr122:
    pushq $0
    pushq $122
    jmp isr_common
.size isr122, .-isr122
.global isr123
.type isr123,@function
isr123:
    pushq $0
    pushq $123
    jmp isr_common
.size isr123, .-isr123
.global isr124
.type isr124,@function
isr124:
    pushq $0
    pushq $124
    jmp isr_common
.size isr124, .-isr124
.global isr125
.type isr125,@function
isr125:
    pushq $0
    pushq $125
    jmp isr_common
.size isr125, .-isr125
.global isr126
.type isr126,@function
isr126:
    pushq $0
    pushq $126
    jmp isr_common
.size isr126, .-isr126
.global isr127
.type isr127,@function
isr127:
    pushq $0
    pushq $127
    jmp isr_common
.size isr127, .-isr127
.global isr128
.type isr128,@function
isr128:
    pushq $0
    pushq $128
    jmp isr_common
.size isr128, .-isr128
.global isr129
.type isr129,@function
isr129:
    pushq $0
    pushq $129
    jmp isr_common
.size isr129, .-isr129
.global isr130
.type isr130,@function
isr130:
    pushq $0
    pushq $130
    jmp isr_common
.size isr130, .-isr130
.global isr131
.type isr131,@function
isr131:
    pushq $0
    pushq $131
    jmp isr_common
.size isr131, .-isr131
.global isr132
.type isr132,@function
isr132:
    pushq $0
    pushq $132
    jmp isr_common
.size isr132, .-isr132
.global isr133
.type isr133,@function
isr133:
    pushq $0
    pushq $133
    jmp isr_common
.size isr133, .-isr133
.global isr134
.type isr134,@function
isr134:
    pushq $0
    pushq $134
    jmp isr_common
.size isr134, .-isr134
.global isr135
.type isr135,@function
isr135:
    pushq $0
    pushq $135
    jmp isr_common
.size isr135, .-isr135
.global isr136
.type isr136,@function
isr136:
    pushq $0
    pushq $136
    jmp isr_common
.size isr136, .-isr136
.global isr137
.type isr137,@function
isr137:
    pushq $0
    pushq $137
    jmp isr_common
.size isr137, .-isr137
.global isr138
.type isr138,@function
isr138:
    pushq $0
    pushq $138
    jmp isr_common
.size isr138, .-isr138
.global isr139
.type isr139,@function
isr139:
    pushq $0
    pushq $139
    jmp isr_common
.size isr139, .-isr139
.global isr140
.type isr140,@function
isr140:
    pushq $0
    pushq $140
    jmp isr_common
.size isr140, .-isr140
.global isr141
.type isr141,@function
isr141:
    pushq $0
    pushq $141
    jmp isr_common
.size isr141, .-isr141
.global isr142
.type isr142,@function
isr142:
    pushq $0
    pushq $142
    jmp isr_common
.size isr142, .-isr142
.global isr143
.type isr143,@function
isr143:
    pushq $0
    pushq $143
    jmp isr_common
.size isr143, .-isr143
.global isr144
.type isr144,@function
isr144:
    pushq $0
    pushq $144
    jmp isr_common
.size isr144, .-isr144
.global isr145
.type isr145,@function
isr145:
    pushq $0
    pushq $145
    jmp isr_common
.size isr145, .-isr145
.global isr146
.type isr146,@function
isr146:
    pushq $0
    pushq $146
    jmp isr_common
.size isr146, .-isr146
.global isr147
.type isr147,@function
isr147:
    pushq $0
    pushq $147
    jmp isr_common
.size isr147, .-isr147
.global isr148
.type isr148,@function
isr148:
    pushq $0
    pushq $148
    jmp isr_common
.size isr148, .-isr148
.global isr149
.type isr149,@function
isr149:
    pushq $0
    pushq $149
    jmp isr_common
.size isr149, .-isr149
.global isr150
.type isr150,@function
isr150:
    pushq $0
    pushq $150
    jmp isr_common
.size isr150, .-isr150
.global isr151
.type isr151,@function
isr151:
    pushq $0
    pushq $151
    jmp isr_common
.size isr151, .-isr151
.global isr152
.type isr152,@function
isr152:
    pushq $0
    pushq $152
    jmp isr_common
.size isr152, .-isr152
.global isr153
.type isr153,@function
isr153:
    pushq $0
    pushq $153
    jmp isr_common
.size isr153, .-isr153
.global isr154
.type isr154,@function
isr154:
    pushq $0
    pushq $154
    jmp isr_common
.size isr154, .-isr154
.global isr155
.type isr155,@function
isr155:
    pushq $0
    pushq $155
    jmp isr_common
.size isr155, .-isr155
.global isr156
.type isr156,@function
isr156:
    pushq $0
    pushq $156
    jmp isr_common
.size isr156, .-isr156
.global isr157
.type isr157,@function
isr157:
    pushq $0
    pushq $157
    jmp isr_common
.size isr157, .-isr157
.global isr158
.type isr158,@function
isr158:
    pushq $0
    pushq $158
    jmp isr_common
.size isr158, .-isr158
.global isr159
.type isr159,@function
isr159:
    pushq $0
    pushq $159
    jmp isr_common
.size isr159, .-isr159
.global isr160
.type isr160,@function
isr160:
    pushq $0
    pushq $160
    jmp isr_common
.size isr160, .-isr160
.global isr161
.type isr161,@function
isr161:
    pushq $0
    pushq $161
    jmp isr_common
.size isr161, .-isr161
.global isr162
.type isr162,@function
isr162:
    pushq $0
    pushq $162
    jmp isr_common
.size isr162, .-isr162
.global isr163
.type isr163,@function
isr163:
    pushq $0
    pushq $163
    jmp isr_common
.size isr163, .-isr163
.global isr164
.type isr164,@function
isr164:
    pushq $0
    pushq $164
    jmp isr_common
.size isr164, .-isr164
.global isr165
.type isr165,@function
isr165:
    pushq $0
    pushq $165
    jmp isr_common
.size isr165, .-isr165
.global isr166
.type isr166,@function
isr166:
    pushq $0
    pushq $166
    jmp isr_common
.size isr166, .-isr166
.global isr167
.type isr167,@function
isr167:
    pushq $0
    pushq $167
    jmp isr_common
.size isr167, .-isr167
.global isr168
.type isr168,@function
isr168:
    pushq $0
    pushq $168
    jmp isr_common
.size isr168, .-isr168
.global isr169
.type isr169,@function
isr169:
    pushq $0
    pushq $169
    jmp isr_common
.size isr169, .-isr169
.global isr170
.type isr170,@function
isr170:
    pushq $0
    pushq $170
    jmp isr_common
.size isr170, .-isr170
.global isr171
.type isr171,@function
isr171:
    pushq $0
    pushq $171
    jmp isr_common
.size isr171, .-isr171
.global isr172
.type isr172,@function
isr172:
    pushq $0
    pushq $172
    jmp isr_common
.size isr172, .-isr172
.global isr173
.type isr173,@function
isr173:
    pushq $0
    pushq $173
    jmp isr_common
.size isr173, .-isr173
.global isr174
.type isr174,@function
isr174:
    pushq $0
    pushq $174
    jmp isr_common
.size isr174, .-isr174
.global isr175
.type isr175,@function
isr175:
    pushq $0
    pushq $175
    jmp isr_common
.size isr175, .-isr175
.global isr176
.type isr176,@function
isr176:
    pushq $0
    pushq $176
    jmp isr_common
.size isr176, .-isr176
.global isr177
.type isr177,@function
isr177:
    pushq $0
    pushq $177
    jmp isr_common
.size isr177, .-isr177
.global isr178
.type isr178,@function
isr178:
    pushq $0
    pushq $178
    jmp isr_common
.size isr178, .-isr178
.global isr179
.type isr179,@function
isr179:
    pushq $0
    pushq $179
    jmp isr_common
.size isr179, .-isr179
.global isr180
.type isr180,@function
isr180:
    pushq $0
    pushq $180
    jmp isr_common
.size isr180, .-isr180
.global isr181
.type isr181,@function
isr181:
    pushq $0
    pushq $181
    jmp isr_common
.size isr181, .-isr181
.global isr182
.type isr182,@function
isr182:
    pushq $0
    pushq $182
    jmp isr_common
.size isr182, .-isr182
.global isr183
.type isr183,@function
isr183:
    pushq $0
    pushq $183
    jmp isr_common
.size isr183, .-isr183
.global isr184
.type isr184,@function
isr184:
    pushq $0
    pushq $184
    jmp isr_common
.size isr184, .-isr184
.global isr185
.type isr185,@function
isr185:
    pushq $0
    pushq $185
    jmp isr_common
.size isr185, .-isr185
.global isr186
.type isr186,@function
isr186:
    pushq $0
    pushq $186
    jmp isr_common
.size isr186, .-isr186
.global isr187
.type isr187,@function
isr187:
    pushq $0
    pushq $187
    jmp isr_common
.size isr187, .-isr187
.global isr188
.type isr188,@function
isr188:
    pushq $0
    pushq $188
    jmp isr_common
.size isr188, .-isr188
.global isr189
.type isr189,@function
isr189:
    pushq $0
    pushq $189
    jmp isr_common
.size isr189, .-isr189
.global isr190
.type isr190,@function
isr190:
    pushq $0
    pushq $190
    jmp isr_common
.size isr190, .-isr190
.global isr191
.type isr191,@function
isr191:
    pushq $0
    pushq $191
    jmp isr_common
.size isr191, .-isr191
.global isr192
.type isr192,@function
isr192:
    pushq $0
    pushq $192
    jmp isr_common
.size isr192, .-isr192
.global isr193
.type isr193,@function
isr193:
    pushq $0
    pushq $193
    jmp isr_common
.size isr193, .-isr193
.global isr194
.type isr194,@function
isr194:
    pushq $0
    pushq $194
    jmp isr_common
.size isr194, .-isr194
.global isr195
.type isr195,@function
isr195:
    pushq $0
    pushq $195
    jmp isr_common
.size isr195, .-isr195
.global isr196
.type isr196,@function
isr196:
    pushq $0
    pushq $196
    jmp isr_common
.size isr196, .-isr196
.global isr197
.type isr197,@function
isr197:
    pushq $0
    pushq $197
    jmp isr_common
.size isr197, .-isr197
.global isr198
.type isr198,@function
isr198:
    pushq $0
    pushq $198
    jmp isr_common
.size isr198, .-isr198
.global isr199
.type isr199,@function
isr199:
    pushq $0
    pushq $199
    jmp isr_common
.size isr199, .-isr199
.global isr200
.type isr200,@function
isr200:
    pushq $0
    pushq $200
    jmp isr_common
.size isr200, .-isr200
.global isr201
.type isr201,@function
isr201:
    pushq $0
    pushq $201
    jmp isr_common
.size isr201, .-isr201
.global isr202
.type isr202,@function
isr202:
    pushq $0
    pushq $202
    jmp isr_common
.size isr202, .-isr202
.global isr203
.type isr203,@function
isr203:
    pushq $0
    pushq $203
    jmp isr_common
.size isr203, .-isr203
.global isr204
.type isr204,@function
isr204:
    pushq $0
    pushq $204
    jmp isr_common
.size isr204, .-isr204
.global isr205
.type isr205,@function
isr205:
    pushq $0
    pushq $205
    jmp isr_common
.size isr205, .-isr205
.global isr206
.type isr206,@function
isr206:
    pushq $0
    pushq $206
    jmp isr_common
.size isr206, .-isr206
.global isr207
.type isr207,@function
isr207:
    pushq $0
    pushq $207
    jmp isr_common
.size isr207, .-isr207
.global isr208
.type isr208,@function
isr208:
    pushq $0
    pushq $208
    jmp isr_common
.size isr208, .-isr208
.global isr209
.type isr209,@function
isr209:
    pushq $0
    pushq $209
    jmp isr_common
.size isr209, .-isr209
.global isr210
.type isr210,@function
isr210:
    pushq $0
    pushq $210
    jmp isr_common
.size isr210, .-isr210
.global isr211
.type isr211,@function
isr211:
    pushq $0
    pushq $211
    jmp isr_common
.size isr211, .-isr211
.global isr212
.type isr212,@function
isr212:
    pushq $0
    pushq $212
    jmp isr_common
.size isr212, .-isr212
.global isr213
.type isr213,@function
isr213:
    pushq $0
    pushq $213
    jmp isr_common
.size isr213, .-isr213
.global isr214
.type isr214,@function
isr214:
    pushq $0
    pushq $214
    jmp isr_common
.size isr214, .-isr214
.global isr215
.type isr215,@function
isr215:
    pushq $0
    pushq $215
    jmp isr_common
.size isr215, .-isr215
.global isr216
.type isr216,@function
isr216:
    pushq $0
    pushq $216
    jmp isr_common
.size isr216, .-isr216
.global isr217
.type isr217,@function
isr217:
    pushq $0
    pushq $217
    jmp isr_common
.size isr217, .-isr217
.global isr218
.type isr218,@function
isr218:
    pushq $0
    pushq $218
    jmp isr_common
.size isr218, .-isr218
.global isr219
.type isr219,@function
isr219:
    pushq $0
    pushq $219
    jmp isr_common
.size isr219, .-isr219
.global isr220
.type isr220,@function
isr220:
    pushq $0
    pushq $220
    jmp isr_common
.size isr220, .-isr220
.global isr221
.type isr221,@function
isr221:
    pushq $0
    pushq $221
    jmp isr_common
.size isr221, .-isr221
.global isr222
.type isr222,@function
isr222:
    pushq $0
    pushq $222
    jmp isr_common
.size isr222, .-isr222
.global isr223
.type isr223,@function
isr223:
    pushq $0
    pushq $223
    jmp isr_common
.size isr223, .-isr223
.global isr224
.type isr224,@function
isr224:
    pushq $0
    pushq $224
    jmp isr_common
.size isr224, .-isr224
.global isr225
.type isr225,@function
isr225:
    pushq $0
    pushq $225
    jmp isr_common
.size isr225, .-isr225
.global isr226
.type isr226,@function
isr226:
    pushq $0
    pushq $226
    jmp isr_common
.size isr226, .-isr226
.global isr227
.type isr227,@function
isr227:
    pushq $0
    pushq $227
    jmp isr_common
.size isr227, .-isr227
.global isr228
.type isr228,@function
isr228:
    pushq $0
    pushq $228
    jmp isr_common
.size isr228, .-isr228
.global isr229
.type isr229,@function
isr229:
    pushq $0
    pushq $229
    jmp isr_common
.size isr229, .-isr229
.global isr230
.type isr230,@function
isr230:
    pushq $0
    pushq $230
    jmp isr_common
.size isr230, .-isr230
.global isr231
.type isr231,@function
isr231:
    pushq $0
    pushq $231
    jmp isr_common
.size isr231, .-isr231
.global isr232
.type isr232,@function
isr232:
    pushq $0
    pushq $232
    jmp isr_common
.size isr232, .-isr232
.global isr233
.type isr233,@function
isr233:
    pushq $0
    pushq $233
    jmp isr_common
.size isr233, .-isr233
.global isr234
.type isr234,@function
isr234:
    pushq $0
    pushq $234
    jmp isr_common
.size isr234, .-isr234
.global isr235
.type isr235,@function
isr235:
    pushq $0
    pushq $235
    jmp isr_common
.size isr235, .-isr235
.global isr236
.type isr236,@function
isr236:
    pushq $0
    pushq $236
    jmp isr_common
.size isr236, .-isr236
.global isr237
.type isr237,@function
isr237:
    pushq $0
    pushq $237
    jmp isr_common
.size isr237, .-isr237
.global isr238
.type isr238,@function
isr238:
    pushq $0
    pushq $238
    jmp isr_common
.size isr238, .-isr238
.global isr239
.type isr239,@function
isr239:
    pushq $0
    pushq $239
    jmp isr_common
.size isr239, .-isr239
.global isr240
.type isr240,@function
isr240:
    pushq $0
    pushq $240
    jmp isr_common
.size isr240, .-isr240
.global isr241
.type isr241,@function
isr241:
    pushq $0
    pushq $241
    jmp isr_common
.size isr241, .-isr241
.global isr242
.type isr242,@function
isr242:
    pushq $0
    pushq $242
    jmp isr_common
.size isr242, .-isr242
.global isr243
.type isr243,@function
isr243:
    pushq $0
    pushq $243
    jmp isr_common
.size isr243, .-isr243
.global isr244
.type isr244,@function
isr244:
    pushq $0
    pushq $244
    jmp isr_common
.size isr244, .-isr244
.global isr245
.type isr245,@function
isr245:
    pushq $0
    pushq $245
    jmp isr_common
.size isr245, .-isr245
.global isr246
.type isr246,@function
isr246:
    pushq $0
    pushq $246
    jmp isr_common
.size isr246, .-isr246
.global isr247
.type isr247,@function
isr247:
    pushq $0
    pushq $247
    jmp isr_common
.size isr247, .-isr247
.global isr248
.type isr248,@function
isr248:
    pushq $0
    pushq $248
    jmp isr_common
.size isr248, .-isr248
.global isr249
.type isr249,@function
isr249:
    pushq $0
    pushq $249
    jmp isr_common
.size isr249, .-isr249
.global isr250
.type isr250,@function
isr250:
    pushq $0
    pushq $250
    jmp isr_common
.size isr250, .-isr250
.global isr251
.type isr251,@function
isr251:
    pushq $0
    pushq $251
    jmp isr_common
.size isr251, .-isr251
.global isr252
.type isr252,@function
isr252:
    pushq $0
    pushq $252
    jmp isr_common
.size isr252, .-isr252
.global isr253
.type isr253,@function
isr253:
    pushq $0
    pushq $253
    jmp isr_common
.size isr253, .-isr253
.global isr254
.type isr254,@function
isr254:
    pushq $0
    pushq $254
    jmp isr_common
.size isr254, .-isr254
.global isr255
.type isr255,@function
isr255:
    pushq $0
    pushq $255
    jmp isr_common
.size isr255, .-isr255
.type isr_common,@function
isr_common:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    andq $-16, %rsp
    call isr_dispatch
    movq %rax, %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
.size isr_common, .-isr_common
.section .rodata
.align 8
isr_stub_table:
    .quad isr0
    .quad isr1
    .quad isr2
    .quad isr3
    .quad isr4
    .quad isr5
    .quad isr6
    .quad isr7
    .quad isr8
    .quad isr9
    .quad isr10
    .quad isr11
    .quad isr12
    .quad isr13
    .quad isr14
    .quad isr15
    .quad isr16
    .quad isr17
    .quad isr18
    .quad isr19
    .quad isr20
    .quad isr21
    .quad isr22
    .quad isr23
    .quad isr24
    .quad isr25
    .quad isr26
    .quad isr27
    .quad isr28
    .quad isr29
    .quad isr30
    .quad isr31
    .quad isr32
    .quad isr33
    .quad isr34
    .quad isr35
    .quad isr36
    .quad isr37
    .quad isr38
    .quad isr39
    .quad isr40
    .quad isr41
    .quad isr42
    .quad isr43
    .quad isr44
    .quad isr45
    .quad isr46
    .quad isr47
    .quad isr48
    .quad isr49
    .quad isr50
    .quad isr51
    .quad isr52
    .quad isr53
    .quad isr54
    .quad isr55
    .quad isr56
    .quad isr57
    .quad isr58
    .quad isr59
    .quad isr60
    .quad isr61
    .quad isr62
    .quad isr63
    .quad isr64
    .quad isr65
    .quad isr66
    .quad isr67
    .quad isr68
    .quad isr69
    .quad isr70
    .quad isr71
    .quad isr72
    .quad isr73
    .quad isr74
    .quad isr75
    .quad isr76
    .quad isr77
    .quad isr78
    .quad isr79
    .quad isr80
    .quad isr81
    .quad isr82
    .quad isr83
    .quad isr84
    .quad isr85
    .quad isr86
    .quad isr87
    .quad isr88
    .quad isr89
    .quad isr90
    .quad isr91
    .quad isr92
    .quad isr93
    .quad isr94
    .quad isr95
    .quad isr96
    .quad isr97
    .quad isr98
    .quad isr99
    .quad isr100
    .quad isr101
    .quad isr102
    .quad isr103
    .quad isr104
    .quad isr105
    .quad isr106
    .quad isr107
    .quad isr108
    .quad isr109
    .quad isr110
    .quad isr111
    .quad isr112
    .quad isr113
    .quad isr114
    .quad isr115
    .quad isr116
    .quad isr117
    .quad isr118
    .quad isr119
    .quad isr120
    .quad isr121
    .quad isr122
    .quad isr123
    .quad isr124
    .quad isr125
    .quad isr126
    .quad isr127
    .quad isr128
    .quad isr129
    .quad isr130
    .quad isr131
    .quad isr132
    .quad isr133
    .quad isr134
    .quad isr135
    .quad isr136
    .quad isr137
    .quad isr138
    .quad isr139
    .quad isr140
    .quad isr141
    .quad isr142
    .quad isr143
    .quad isr144
    .quad isr145
    .quad isr146
    .quad isr147
    .quad isr148
    .quad isr149
    .quad isr150
    .quad isr151
    .quad isr152
    .quad isr153
    .quad isr154
    .quad isr155
    .quad isr156
    .quad isr157
    .quad isr158
    .quad isr159
    .quad isr160
    .quad isr161
    .quad isr162
    .quad isr163
    .quad isr164
    .quad isr165
    .quad isr166
    .quad isr167
    .quad isr168
    .quad isr169
    .quad isr170
    .quad isr171
    .quad isr172
    .quad isr173
    .quad isr174
    .quad isr175
    .quad isr176
    .quad isr177
    .quad isr178
    .quad isr179
    .quad isr180
    .quad isr181
    .quad isr182
    .quad isr183
    .quad isr184
    .quad isr185
    .quad isr186
    .quad isr187
    .quad isr188
    .quad isr189
    .quad isr190
    .quad isr191
    .quad isr192
    .quad isr193
    .quad isr194
    .quad isr195
    .quad isr196
    .quad isr197
    .quad isr198
    .quad isr199
    .quad isr200
    .quad isr201
    .quad isr202
    .quad isr203
    .quad isr204
    .quad isr205
    .quad isr206
    .quad isr207
    .quad isr208
    .quad isr209
    .quad isr210
    .quad isr211
    .quad isr212
    .quad isr213
    .quad isr214
    .quad isr215
    .quad isr216
    .quad isr217
    .quad isr218
    .quad isr219
    .quad isr220
    .quad isr221
    .quad isr222
    .quad isr223
    .quad isr224
    .quad isr225
    .quad isr226
    .quad isr227
    .quad isr228
    .quad isr229
    .quad isr230
    .quad isr231
    .quad isr232
    .quad isr233
    .quad isr234
    .quad isr235
    .quad isr236
    .quad isr237
    .quad isr238
    .quad isr239
    .quad isr240
    .quad isr241
    .quad isr242
    .quad isr243
    .quad isr244
    .quad isr245
    .quad isr246
    .quad isr247
    .quad isr248
    .quad isr249
    .quad isr250
    .quad isr251
    .quad isr252
    .quad isr253
    .quad isr254
    .quad isr255
.section .note.GNU-stack,"",@progbits
