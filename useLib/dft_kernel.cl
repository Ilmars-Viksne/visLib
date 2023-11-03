__kernel void dft_R1SPN(__global const float* inputReal, __global float* outputReal) {
    int gid = get_global_id(0);
    int sampleSize = 2* get_global_size(0) - 2;
    float angle, cosVal, sinVal;
    const float PI2 = 6.28319f;

    float angleK = PI2 * gid / sampleSize;
    float cosValK = cos(angleK);
    float sinValK = sin(angleK);

    outputReal[gid] = 0.0f;
    float outputImag = 0.0f;

    for (int n = 0; n < sampleSize; n++) {
        angle = angleK * n;
        cosVal = cos(angle);
        sinVal = sin(angle);

        outputReal[gid] += inputReal[n] * cosVal;
        outputImag -= inputReal[n] * sinVal;
    }

    if(gid == 0){
        outputReal[gid] *= (1.0f / sampleSize);
        outputImag *= (1.0f / sampleSize);
    }
    else{
        outputReal[gid] *= (2.0f / sampleSize);
        outputImag *= (2.0f / sampleSize);
    }

    outputReal[gid] = outputReal[gid]*outputReal[gid] + outputImag*outputImag;

}

__kernel void dft_R1SP(__global const float* inputReal, __global float* outputReal) {
    int gid = get_global_id(0);
    int sampleSize = 2* get_global_size(0) - 2;
    float angle, cosVal, sinVal;
    const float PI2 = 6.28319f;

    float angleK = PI2 * gid / sampleSize;
    float cosValK = cos(angleK);
    float sinValK = sin(angleK);

    outputReal[gid] = 0.0f;
    float outputImag = 0.0f;

    for (int n = 0; n < sampleSize; n++) {
        angle = angleK * n;
        cosVal = cos(angle);
        sinVal = sin(angle);

        outputReal[gid] += inputReal[n] * cosVal;
        outputImag -= inputReal[n] * sinVal;
    }

    outputReal[gid] = outputReal[gid]*outputReal[gid] + outputImag*outputImag;

}