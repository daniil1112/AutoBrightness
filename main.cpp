#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <chrono>


using namespace std;


vector<unsigned char> readFile(char *fileName, char *pType, int *width, int *height, int *depth) {
    FILE *f = fopen(fileName, "rb");

    if (f == nullptr){
        cout << " file not found" << endl;
        exit(404);
    }

    fscanf(f, "%s\n", pType);

    fscanf(f, "%d %d\n", width, height);

    fscanf(f, "%d\n", depth);


    int size = *width * *height;

    vector<unsigned char> res;


    if (strcmp(pType, "P5") == 0) {
        res.reserve(size);
        fread(&res[0], 1, size, f);
    } else if (strcmp(pType, "P6") == 0){
        res.reserve(size*3);
        fread(&res[0], 1, size*3, f);
    } else {
        cout << "File has unsupported format" << endl;
        fclose(f);
        exit(403);
    }
    fclose(f);
    return res;

};


void getMinMax(char *type, int width, int height, float skip, vector<unsigned char>& pixels, unsigned int *ch1Min, unsigned int *ch1Max, int streams){
    vector <unsigned long> ch1Cnt;
    ch1Cnt.resize(256);

    unsigned long ch1CntCurrent = 0;
    unsigned long skippedPixels = (float) skip * width * height;
    unsigned long size = width * height;

    unsigned int min1 = 256;
    unsigned int min2 = 256;
    unsigned int min3 = 256;
    unsigned int max1 = 256;
    unsigned int max2 = 256;
    unsigned int max3 = 256;

    if (strcmp(type, "P6") == 0) {
        vector <unsigned long> ch2Cnt;
        vector <unsigned long> ch3Cnt;
        unsigned long ch2CntCurrent = 0;
        unsigned long ch3CntCurrent = 0;
        ch2Cnt.resize(256);
        ch3Cnt.resize(256);

        #pragma omp parallel sections num_threads(streams)
        {
            #pragma omp section
            {
                for (unsigned long i = 0; i < size*3; i = i + 3){
                    ch1Cnt[pixels[i]]++;
                }
            }
            #pragma omp section
            {
                for (unsigned long i = 1; i < size*3; i = i + 3){
                    ch2Cnt[pixels[i]]++;
                }
            }
            #pragma omp section
            {
                for (unsigned long i = 2; i < size*3; i = i + 3){
                    ch3Cnt[pixels[i]]++;
                }
            }
        }

        for (unsigned int i = 0; i < 256; i++){
            ch1CntCurrent += ch1Cnt[i];
            ch2CntCurrent += ch2Cnt[i];
            ch3CntCurrent += ch3Cnt[i];

            if (min1 == 256 && ch1CntCurrent > skippedPixels){
                min1 = i;
            }
            if (max1 == 256 &&(ch1CntCurrent > size - skippedPixels || ch1CntCurrent == size)){
                max1 = i;
            }
            if (min2 == 256 && ch2CntCurrent > skippedPixels){
                min2 = i;
            }
            if (max2== 256 &&(ch2CntCurrent > size - skippedPixels || ch2CntCurrent == size)){
                max2 = i;
            }
            if (min3 == 256 && ch3CntCurrent > skippedPixels){
                min3 = i;
            }
            if (max3== 256 &&(ch3CntCurrent > size - skippedPixels || ch3CntCurrent == size)){
                max3 = i;
            }
            *ch1Min = min(min(min1, min2), min3);
            *ch1Max = max(max(max1, max2), max3);
        }
    } else {
        for (unsigned long i = 0; i < size; i++){
            ch1Cnt[pixels[i]]++;
        }
        for (unsigned int i = 0; i < 256; i++){
            ch1CntCurrent += ch1Cnt[i];

            if (*ch1Min == 256 && ch1CntCurrent > skippedPixels){
                *ch1Min = i;
            }
            if (*ch1Max == 256 &&(ch1CntCurrent > size - skippedPixels || ch1CntCurrent == size)){
                *ch1Max = i;
            }
        }
    }


}

void writeFile(char *type, char *fileName, int width, int height, vector <unsigned char> &res){
    FILE *f = fopen(fileName, "wb");


    fprintf(f, "%s\n", type);

    fprintf(f, "%d %d\n", width, height);

    fprintf(f, "%d\n", 255);

    if (strcmp(type, "P5") == 0){
        fwrite(&res[0], width, height, f);
    } else {
        fwrite(&res[0], width*3, height, f);
    }

    fclose(f);
}

unsigned char convert(unsigned char val, unsigned int h, unsigned int l){
    if (h - l == 0){
        return 255;
    }

    if (val <= l){
        return 0;
    }
    if (val >= h){
        return 255;
    }

    return ((val - l)*255.0/(h-l));
}


int main(int argc, char *argv[]) {
    if (argc < 5){
        cout << "program expect 4 arguments" << endl;
        exit(403);
    }

    int streams = stoi(argv[1]);


    char *inputFileName = argv[2];
    char *outputFileName = argv[3];
    float skip = stof(argv[4]);
    char pType[50];
    int width = 0;
    int height = 0;
    int depth = 255;

    unsigned int ch1Min = 256;
    unsigned int ch1Max = 256;

    if (skip >= 0.5 || skip < 0){
        cout << "invalid skip parameter" << endl;
        exit(403);
    }

    vector<unsigned char> pixels;

    pixels = readFile(inputFileName, pType, &width, &height, &depth);

    auto begin = std::chrono::steady_clock::now();

    getMinMax(pType, width, height, skip, pixels, &ch1Min, &ch1Max, streams);

    unsigned long size = width * height;

    if (strcmp(pType, "P6") == 0) {
        size *= 3;
    }

    #pragma omp parallel num_threads(streams)
    {
        #pragma omp for schedule(static)
        for(unsigned long i = 0; i < size; i++){
            pixels[i] = convert(pixels[i], ch1Max, ch1Min);
        }
    }

    auto end = std::chrono::steady_clock::now();

    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    writeFile(pType, outputFileName, width, height, pixels);

    printf("Time (%i thread(s)): %g ms\n", streams, (double) elapsed_ms.count());

    return 0;
}
