#ifndef TGA_LOADER_H
#define TGA_LOADER_H

#include "sandboxdef.h"


#define TGA_RGBA 0x00
#define TGA_BGRA 0x01

struct tga_image_header
{
    u8  IdLength;
    u8  ColorMapType;
    u8  ImageType;
    u16 ColorMapFirstEntryIndex;
    u16 ColorMapLength;
    u8  ColorMapEntrySize;
    u16 ImageOriginX;
    u16 ImageOriginY;
    u16 ImageWidth;
    u16 ImageHeight;
    u8  ImagePixelDepth;
    u8  ImageDescriptor;
};

struct tga_image_extension_area
{
    u16     ExtensionSize;
    u8      AuthorName[41];
    u8      AuthorComments[324];
    u16     Date[6];
    u8      JobName[41];
    u16     JobTime[3];
    u8      SoftwareId[41];
    u8      SoftwareVersion[3];
    u32     KeyColor;
    u16     PixelAspect[2];
    u16     GammaValue[2];
    u32     ColorCorrectionOffset;
    u32     PostageStampOffset;
    u32     ScanLineOffset;
    u8      AttributesType;
    void    *ScanLineTable;
    void    *PostageStampImage;
    u16     ColorCorrectionTable[1024];
};

struct tga_image_footer
{
    u32     ExtensionAreaOffset;
    u32     DeveloperDirectoryOffset;
    char    SignatureWithTerminator[18];
};

struct tga_image
{
    struct  tga_image_header Header;
    void    *ImageId;
    void    *ColorMapData;
    u32     *ImageData;

    /* NOT YET POPULATED */
    void    *DeveloperFields;
    void    *DeveloperDirectory;
    struct  tga_image_extension_area ExtensionArea;
    struct  tga_image_footer Footer;
    
    u8      Original;
};


struct tga_image    *LoadTGA(const char *Path, u32 format);
void                FreeTGA(struct tga_image *Image);
u32                 ComputePixel(u8 *p, u32 bytes, u32 format);



#ifndef LOG
#define LOG(x) puts(x)
#endif


#ifdef TGA_LOADER_IMPL
#include <stdio.h>

struct tga_image*
LoadTGA(const char* Path, u32 format){
    struct tga_image* Image = malloc(sizeof(struct tga_image));
    memset(Image,0,sizeof(struct tga_image));

    FILE *fp = fopen(Path,"r");
    if(fp == NULL){
        LOG("Unable to open file");
        FreeTGA(Image);
        return NULL;
    }

    fseek(fp, -26, SEEK_END);
    fread(&Image->Footer, 1, sizeof(struct tga_image_footer), fp);
    
    if(strncmp(Image->Footer.SignatureWithTerminator, "TRUEVISION-XFILE", 16) == 0)
    { 
        Image->Original = 0;
    } 
    else
    {
        Image->Original = 1;
        memset(&Image->Footer, 0, sizeof(struct tga_image_footer));
    }

    fseek(fp, 0, SEEK_SET);
    
    Image->Header.IdLength = fgetc(fp);
    Image->Header.ColorMapType = fgetc(fp);
    Image->Header.ImageType = fgetc(fp);
    fread(&Image->Header.ColorMapFirstEntryIndex,2,1,fp);
    fread(&Image->Header.ColorMapLength,2,1,fp);
    Image->Header.ColorMapEntrySize = fgetc(fp);
    fread(&Image->Header.ImageOriginX,2,1,fp);
    fread(&Image->Header.ImageOriginY,2,1,fp);
    fread(&Image->Header.ImageWidth,2,1,fp);
    fread(&Image->Header.ImageHeight,2,1,fp);
    Image->Header.ImagePixelDepth = fgetc(fp);
    Image->Header.ImageDescriptor = fgetc(fp);

   
    if(Image->Header.IdLength != 0)
    {
    Image->ImageId = malloc(Image->Header.IdLength);
    fread(&Image->ImageId, 1, Image->Header.IdLength, fp);
    }
    if(Image->Header.ColorMapLength != 0)
    {
    Image->ColorMapData = malloc(Image->Header.ColorMapLength);
    fread(&Image->ColorMapData, 1, Image->Header.ColorMapLength, fp);
    }

    Image->ImageData = malloc(Image->Header.ImageWidth * Image->Header.ImageHeight * sizeof(u32));
    if(Image->ImageData == NULL)
    {
        FreeTGA(Image);
        return NULL;
    }


    u32 size = Image->Header.ImagePixelDepth / 8;
    if(Image->Header.ImageType == 2)
    {
    for(u32 i = 0; i < Image->Header.ImageWidth * Image->Header.ImageHeight; i++){
        u32 p = 0;
    
            if(size != fread(&p , 1, size, fp))
            {
                LOG("Unexpected EOF...");
                FreeTGA(Image);
                return NULL;
            }

            Image->ImageData[i] = ComputePixel(&p,size,format);

        }
    }
    else if (Image->Header.ImageType == 10) //COMPRESSED TRUE-COLOR
    {
        for(u32 i = 0; i < Image->Header.ImageWidth * Image->Header.ImageHeight;)
        {
            u8 p[5];

            if(size+1 != fread(&p, 1, size+1, fp))
            {
                LOG("Unexpected EOF...");
                FreeTGA(Image);
                return NULL;
            }
            int j = p[0] & 0x7F;
            Image->ImageData[i] = ComputePixel(&(p[1]), size, format);
            i++;
            if(p[0] & 0x80)
            {
                for(u32 n = 0; n < j; n++)
                {
                    Image->ImageData[i] = ComputePixel(&(p[1]), size, format);
                    i++;
                }
            }
            else
            {
                for(u32 n = 0; n < j; n++)
                {
                    u32 pp = 0;
                    if(size != fread(&pp,1,size,fp))
                    {
                        LOG("Unexpected EOF...");
                        FreeTGA(Image);
                        return NULL;
                    }
                    Image->ImageData[i] = ComputePixel(&pp, size, format);
                    i++;
                }
            }
        }
    }

    if(!Image->Original){      
        //TODO: Load ExtensionArea and DeveloperArea
    }
    
    fclose(fp);
    
    return Image;
}

u32
ComputePixel(u8 *input, u32 bytes, u32 format)
{
    u32 res = 0;
    switch(bytes)
    {
        case 4:
            switch(format)
            {
                case TGA_BGRA:
                    res = *((u32*)input);
                    break;
                case TGA_RGBA:
                    res = ((input[2] & 0xFF) << 24) 
                        | ((input[1] & 0xFF) << 16) 
                        | ((input[0] & 0xFF) << 8) 
                        | (input[3] & 0xFF);
                    break;
            }
            break;
        case 3:
            switch(format)
            {
                case TGA_BGRA:
                    res = *((u32*)input);
                    break;
                case TGA_RGBA:
                     res = ((input[2] & 0xFF) << 24) 
                        | ((input[1] & 0xFF) << 16) 
                        | ((input[0] & 0xFF) << 8) 
                        | 0; 
                    break;
            }
            break;
        case 2:
            LOG("not yet implemented... image will be black");
            break;
    }
    return res;
}

void
FreeTGA(struct tga_image *Image){
    if(Image)
    {
    if(Image->ImageId)
        free(Image->ImageId);
    if(Image->ColorMapData)
        free(Image->ColorMapData);
    if(Image->ImageData)
        free(Image->ImageData);
    free(Image);
    Image = NULL;
    }
}
#endif

#endif
