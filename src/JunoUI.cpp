#pragma once
#include "InstrumentJuno.h"
#include "juno_patches.h"
#include <algorithm>

// Helper: Draw slider structure (vertical line + 4 horizontal lines)
void drawSlider(U8G2 &u8g2, uint8_t centerX, uint8_t sliderTop, uint8_t sliderBottom, uint8_t lineY[4])
{
    uint8_t sliderHeight = sliderBottom - sliderTop;

    u8g2.drawVLine(centerX, sliderTop, sliderHeight + 1);

    uint8_t step = sliderHeight / 3;
    for (int i = 0; i < 3; i++)
    {
        lineY[i] = sliderTop + (i * step);
        uint8_t lineWidth = (i == 0) ? 7 : 5;
        u8g2.drawHLine(centerX - lineWidth / 2, lineY[i], lineWidth);
    }
    lineY[3] = sliderBottom;
    u8g2.drawHLine(centerX - 3, lineY[3], 7);
}

// Helper: Draw HPF triangles (5px tall, quantized positions)
void drawHPFTriangles(U8G2 &u8g2, uint8_t centerX, uint8_t tipY)
{
    // Left triangle (pointing right)
    u8g2.drawPixel(centerX - 4, tipY + 1);
    u8g2.drawPixel(centerX - 5, tipY);
    u8g2.drawPixel(centerX - 5, tipY + 2);
    u8g2.drawPixel(centerX - 6, tipY - 1);
    u8g2.drawPixel(centerX - 6, tipY + 3);

    // Right triangle (pointing left)
    u8g2.drawPixel(centerX + 4, tipY + 1);
    u8g2.drawPixel(centerX + 5, tipY);
    u8g2.drawPixel(centerX + 5, tipY + 2);
    u8g2.drawPixel(centerX + 6, tipY - 1);
    u8g2.drawPixel(centerX + 6, tipY + 3);
}

// Helper: Draw single small triangle (2px wide, 3px tall)
void drawSliderTriangle(U8G2 &u8g2, uint8_t centerX, uint8_t tipY, bool leftSide)
{
    if (leftSide)
    {
        // Left triangle (pointing right, tip at centerX-1)
        u8g2.drawPixel(centerX - 4, tipY);
        u8g2.drawPixel(centerX - 5, tipY - 1);
        u8g2.drawPixel(centerX - 5, tipY);
        u8g2.drawPixel(centerX - 5, tipY + 1);
    }
    else
    {
        // Right triangle (pointing left, tip at centerX+1)
        u8g2.drawPixel(centerX + 4, tipY);
        u8g2.drawPixel(centerX + 5, tipY - 1);
        u8g2.drawPixel(centerX + 5, tipY);
        u8g2.drawPixel(centerX + 5, tipY + 1);
    }
}

// Helper: Draw dual slider (VCF/LFO style) with two triangles and labels
void drawDualSlider(U8G2 &u8g2, uint8_t centerX, uint8_t leftTipY, uint8_t rightTipY, char leftLabel, char rightLabel, uint8_t graphBottom, uint8_t graphTop)
{
    // Left triangle
    drawSliderTriangle(u8g2, centerX, leftTipY, true);

    // Left label with clamp
    u8g2.setFont(u8g2_font_4x6_tr);
    uint8_t leftLabelY = std::max(std::min(graphBottom - 2, leftTipY + 3), graphTop + 6);
    u8g2.setCursor(centerX - 9, leftLabelY);
    u8g2.print(leftLabel);

    // Right triangle
    drawSliderTriangle(u8g2, centerX, rightTipY, false);

    // Right label with clamp
    uint8_t rightLabelY = std::max(std::min(graphBottom - 2, rightTipY + 3), graphTop + 6);
    u8g2.setCursor(centerX + 7, rightLabelY);
    u8g2.print(rightLabel);
}

void InstrumentJuno::drawUI(U8G2 &u8g2, uint8_t y_offset)
{
    const uint8_t screenWidth = 128;
    const uint8_t screenHeight = 64;
    const uint8_t headerBoxHeight = 8;
    const uint8_t graphHeight = 25;
    const uint8_t headerY = y_offset + 14;
    const uint8_t graphTop = headerY + headerBoxHeight;
    const uint8_t graphBottom = graphTop + graphHeight;

    // --- PATCH NAME ---
    u8g2.setFont(u8g2_font_tenfatguys_tf);
    drawWrappedPatchName(u8g2, 0, y_offset + 11, juno_patch_names[patch]);

    // --- COLUMN HEADER BOXES ---
    u8g2.setFont(u8g2_font_5x7_tr);
    const uint8_t colWidths[] = {33, 17, 22, 22, 33};
    const char *labels[] = {"DCO", "HPF", "VCF", "LFO", "ENV"};

    uint8_t x = 0;
    for (int i = 0; i < 5; i++)
    {
        u8g2.setDrawColor(1);
        u8g2.drawBox(x + 1, headerY, colWidths[i] - 1, headerBoxHeight);
        u8g2.drawVLine(x, headerY + headerBoxHeight, graphHeight);
        u8g2.setDrawColor(0);
        u8g2.setCursor(x + colWidths[i] / 2 - 6, headerY + 7);
        u8g2.print(labels[i]);
        x += colWidths[i];
    }
    u8g2.setDrawColor(1);
    u8g2.drawVLine(x, headerY + headerBoxHeight, graphHeight);

    // Horizontal bottom line
    u8g2.drawHLine(0, screenHeight - 6, screenWidth);

    // --- COLUMN 1: DCO (31px) ---
    uint8_t dcoX = 0;
    uint8_t dcoWidth = colWidths[0];
    uint8_t dcoTopHeight = 8;
    uint8_t dcoSplitY = graphTop + dcoTopHeight;

    u8g2.drawHLine(dcoX + 1, dcoSplitY + 1, dcoWidth - 1);

    uint8_t dutyMargin = 1;
    uint8_t dutyLeft = dcoX + 1 + dutyMargin;
    uint8_t dutyTop = graphTop + dutyMargin;
    uint8_t dutyRight = dcoX + dcoWidth - 1 - dutyMargin;
    uint8_t dutyBottom = dcoSplitY - dutyMargin;
    uint8_t dutyWidth = dutyRight - dutyLeft;
    uint8_t dutyHeight = dutyBottom - dutyTop;

    float active_pwm = 0.0f;
    if (state.dco_pwm > 0.02f)
    {
        active_pwm = (state.dco_pwm - 0.02f) / 0.98f;
    }

    if (active_pwm <= 0.01f)
    {
        u8g2.drawHLine(dutyLeft, dutyBottom, dutyWidth);
    }
    else
    {
        uint8_t pulseWidth = (uint8_t)(dutyWidth * active_pwm);
        if (pulseWidth < 1)
            pulseWidth = 1;
        uint8_t pulseStart = dutyLeft + (dutyWidth - pulseWidth) / 2;
        uint8_t pulseEnd = pulseStart + pulseWidth;

        u8g2.drawHLine(dutyLeft, dutyBottom, pulseStart - dutyLeft);
        u8g2.drawVLine(pulseStart, dutyTop, dutyHeight + 1);
        u8g2.drawHLine(pulseStart, dutyTop, pulseWidth);
        u8g2.drawVLine(pulseEnd, dutyTop, dutyHeight);
        u8g2.drawHLine(pulseEnd, dutyBottom, dutyRight - pulseEnd);
    }

    uint8_t barWidth = 5;
    uint8_t totalBarWidth = barWidth * 3;
    uint8_t barSpacing = (dcoWidth - 2 - totalBarWidth) / 4;
    uint8_t barMaxHeight = graphBottom - dcoSplitY - 5;

    uint8_t sawBarX = dcoX + 1 + barSpacing;
    uint8_t sawBarHeight = (uint8_t)(state.saw_level * barMaxHeight);
    u8g2.drawLine(sawBarX, graphBottom - 3, sawBarX + barWidth, graphBottom - 3 - sawBarHeight);
    u8g2.drawVLine(sawBarX + barWidth, graphBottom - 2 - sawBarHeight, sawBarHeight);

    uint8_t subBarX = sawBarX + barWidth + barSpacing;
    uint8_t subBarHeight = (uint8_t)(state.dco_sub * barMaxHeight);
    u8g2.drawVLine(subBarX, graphBottom - 2 - subBarHeight, subBarHeight);
    u8g2.drawHLine(subBarX, graphBottom - 3 - subBarHeight, barWidth+1);
    u8g2.drawVLine(subBarX + barWidth, graphBottom - 2 - subBarHeight, subBarHeight);

    uint8_t noiseBarX = subBarX + barWidth + barSpacing;
    uint8_t noiseBarHeight = std::max((uint8_t)(state.dco_noise * (barMaxHeight+1)), (uint8_t)1);
    u8g2.drawBox(noiseBarX, graphBottom - 2 - noiseBarHeight, barWidth, noiseBarHeight);

    // --- COLUMN 2: HPF (17px) ---
    uint8_t hpfX = colWidths[0];
    uint8_t hpfWidth = colWidths[1];
    uint8_t hpfCenterX = hpfX + hpfWidth / 2;
    uint8_t hpfSliderTop = graphTop + 3;
    uint8_t hpfSliderBottom = graphBottom - 3;

    uint8_t hpfLineY[4];
    drawSlider(u8g2, hpfCenterX, hpfSliderTop, hpfSliderBottom, hpfLineY);

    uint8_t hpfLineIdx = 3 - state.hpf;
    uint8_t hpfTriangleTipY = hpfLineY[hpfLineIdx] - 1;

    drawHPFTriangles(u8g2, hpfCenterX, hpfTriangleTipY);

    // --- COLUMN 3: VCF (21px) ---
    uint8_t vcfX = hpfX + hpfWidth;
    uint8_t vcfWidth = colWidths[2];
    uint8_t vcfCenterX = vcfX + vcfWidth / 2;
    uint8_t vcfSliderTop = graphTop + 3;
    uint8_t vcfSliderBottom = graphBottom - 4;

    uint8_t vcfLineY[4];
    drawSlider(u8g2, vcfCenterX, vcfSliderTop, vcfSliderBottom, vcfLineY);

    float freqPos = 1.0f - state.vcf_freq;
    uint8_t freqTipY = (uint8_t)(vcfSliderTop + freqPos * (vcfSliderBottom - vcfSliderTop) + 0.5f);

    float resPos = 1.0f - state.vcf_res;
    uint8_t resTipY = (uint8_t)(vcfSliderTop + resPos * (vcfSliderBottom - vcfSliderTop) + 0.5f);

    drawDualSlider(u8g2, vcfCenterX, freqTipY, resTipY, 'F', 'R', graphBottom, graphTop);

    // --- COLUMN 4: LFO (21px) ---
    uint8_t lfoX = vcfX + vcfWidth;
    uint8_t lfoWidth = colWidths[3];
    uint8_t lfoCenterX = lfoX + lfoWidth / 2;
    uint8_t lfoSliderTop = graphTop + 3;
    uint8_t lfoSliderBottom = graphBottom - 4;

    uint8_t lfoLineY[4];
    drawSlider(u8g2, lfoCenterX, lfoSliderTop, lfoSliderBottom, lfoLineY);

    float lfoFreqPos = 1.0f - state.lfo_rate;
    uint8_t lfoFreqTipY = (uint8_t)(lfoSliderTop + lfoFreqPos * (lfoSliderBottom - lfoSliderTop) + 0.5f);

    float lfoAmpPos = 1.0f - state.vcf_lfo;
    uint8_t lfoAmpTipY = (uint8_t)(lfoSliderTop + lfoAmpPos * (lfoSliderBottom - lfoSliderTop) + 0.5f);

    drawDualSlider(u8g2, lfoCenterX, lfoFreqTipY, lfoAmpTipY, 'F', 'A', graphBottom, graphTop);

    // --- COLUMN 5: ENV (33px) ---
    uint8_t envX = lfoX + lfoWidth;
    uint8_t envWidth = colWidths[4];
    uint8_t envGraphLeft = envX + 2;
    uint8_t envGraphRight = envX + envWidth - 2;
    uint8_t envGraphWidth = envGraphRight - envGraphLeft;
    uint8_t envGraphTop = graphTop + 2;
    uint8_t envGraphBottom = graphBottom - 4;
    uint8_t envGraphHeight = envGraphBottom - envGraphTop;

    uint8_t maxSegmentWidth = envGraphWidth / 3.25;

    uint8_t attackWidth = std::max((uint8_t)1, (uint8_t)(state.env_a * maxSegmentWidth));
    uint8_t decayWidth = std::max((uint8_t)1, (uint8_t)(state.env_d * maxSegmentWidth));
    uint8_t releaseWidth = std::max((uint8_t)1, (uint8_t)(state.env_r * maxSegmentWidth));
    uint8_t sustainWidth = std::max((uint8_t)1, (uint8_t)(envGraphWidth - attackWidth - decayWidth - releaseWidth));

    uint8_t attackX = envGraphLeft;
    uint8_t attackY = envGraphBottom;
    uint8_t decayX = attackX + attackWidth;
    uint8_t decayY = envGraphTop;
    uint8_t sustainX = decayX + decayWidth;
    uint8_t sustainY = envGraphTop + (uint8_t)((1.0f - state.env_s) * envGraphHeight);
    uint8_t releaseStartX = sustainX + sustainWidth;
    uint8_t releaseStartY = sustainY;
    uint8_t endX = std::min(envGraphRight, (uint8_t)(releaseStartX + releaseWidth));
    uint8_t endY = envGraphBottom;

    if (endX >= envGraphRight)
        releaseStartX = endX - 1;

    u8g2.drawLine(attackX, attackY, decayX, decayY);
    u8g2.drawLine(decayX, decayY, sustainX, sustainY);
    u8g2.drawLine(sustainX, sustainY, releaseStartX, releaseStartY);
    u8g2.drawLine(releaseStartX, releaseStartY, endX, endY);

    u8g2.setDrawColor(1);
}
