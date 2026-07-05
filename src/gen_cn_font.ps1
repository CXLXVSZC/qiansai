param(
    [string]$InputPath = 'C:\Users\24038\Desktop\新建 文本文档.txt',
    [string]$OutputPath = (Join-Path $PSScriptRoot 'lcd_cn_font.h')
)

$codeMap = @{
    'CAB6' = 0xE8AF86
    'B1F0' = 0xE588AB
    'B5BD' = 0xE588B0
    'BAB8' = 0xE7848A
    'C5CC' = 0xE79B98
    'B9FD' = 0xE8BF87
    'B8F6' = 0xE4B8AA
    'D7F8' = 0xE59D90
    'B1EA' = 0xE6A087
    'A3BA' = 0xEFBC9A
    'A3A8' = 0xEFBC88
    'A3A9' = 0xEFBC89
    'CEB4' = 0xE69CAA
    'D2D1' = 0xE5B7B2
}

$glyphs = New-Object System.Collections.Generic.List[object]
$cur = $null

foreach ($line in Get-Content -LiteralPath $InputPath) {
    if ($line -match 'ASCII.*?([0-9A-F]{4})') {
        if ($null -ne $cur) { $glyphs.Add($cur) }
        $cur = [ordered]@{
            code = $matches[1]
            bytes = New-Object System.Collections.Generic.List[string]
        }
        continue
    }

    if ($null -ne $cur) {
        foreach ($m in [regex]::Matches($line, '0x[0-9a-fA-F]{2}')) {
            [void]$cur.bytes.Add($m.Value.ToLowerInvariant())
        }
    }
}

if ($null -ne $cur) { $glyphs.Add($cur) }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('#ifndef LCD_CN_FONT_H')
[void]$sb.AppendLine('#define LCD_CN_FONT_H')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#include <stdint.h>')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#define LCD_CN_GLYPH_SRC_W 24')
[void]$sb.AppendLine('#define LCD_CN_GLYPH_SRC_H 24')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('typedef struct {')
[void]$sb.AppendLine('    const char *utf8;')
[void]$sb.AppendLine('    const uint8_t data[72];')
[void]$sb.AppendLine('} lcd_cn_glyph_t;')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('static const lcd_cn_glyph_t g_lcd_cn_glyphs[] = {')

foreach ($g in $glyphs) {
    if (-not $codeMap.ContainsKey($g.code)) { continue }
    $hex = $codeMap[$g.code].ToString('X6')
    $utf8 = '\x' + $hex.Substring(0,2) + '\x' + $hex.Substring(2,2) + '\x' + $hex.Substring(4,2)
    [void]$sb.AppendLine("    {`"$utf8`", {")
    for ($i = 0; $i -lt 72; $i += 8) {
        $end = [Math]::Min($i + 7, 71)
        $chunk = $g.bytes[$i..$end]
        [void]$sb.AppendLine('        ' + ($chunk -join ',') + ',')
    }
    [void]$sb.AppendLine('    }},')
}

[void]$sb.AppendLine('};')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('#endif /* LCD_CN_FONT_H */')

Set-Content -LiteralPath $OutputPath -Value $sb.ToString() -Encoding UTF8
