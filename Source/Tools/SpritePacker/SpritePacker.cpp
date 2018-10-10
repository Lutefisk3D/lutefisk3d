//
// Copyright (c) 2008-2018 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Lutefisk3D/Core/Context.h>
#include <Lutefisk3D/Core/ProcessUtils.h>
#include <Lutefisk3D/Core/StringUtils.h>
#include <Lutefisk3D/IO/File.h>
#include <Lutefisk3D/IO/FileSystem.h>
#include <Lutefisk3D/IO/Log.h>
#include <Lutefisk3D/Resource/Image.h>
#include <Lutefisk3D/Resource/XMLElement.h>
#include <Lutefisk3D/Resource/XMLFile.h>

#ifdef WIN32
#include <windows.h>
#endif

#define STBRP_LARGE_RECTS
#if !URHO3D_STATIC
#   define STB_RECT_PACK_IMPLEMENTATION
#endif
#include <STB/stb_rect_pack.h>

using namespace Urho3D;

// number of nodes allocated to each packer info.  since this packer is not suited for real time purposes we can over allocate.
const int PACKER_NUM_NODES = 4096;
const int MAX_TEXTURE_SIZE = 2048;

int main(int argc, char** argv);
void Run(QStringList& arguments);

class PackerInfo : public RefCounted
{
public:
    QString path;
    QString name;
    int x{};
    int y{};
    int offsetX{};
    int offsetY{};
    int width{};
    int height{};
    int frameWidth{};
    int frameHeight{};

    PackerInfo(const QString& path_, const QString& name_) :
        path(path_),
        name(name_)
    {
    }

    ~PackerInfo() override = default;
};

void Help()
{
    ErrorExit("Usage: SpritePacker -options <input file> <input file> <output png file>\n"
        "\n"
        "Options:\n"
        "-h Shows this help message.\n"
        "-px Adds x pixels of padding per image to width.\n"
        "-py Adds y pixels of padding per image to height.\n"
        "-ox Adds x pixels to the horizontal position per image.\n"
        "-oy Adds y pixels to the horizontal position per image.\n"
        "-frameHeight Sets a fixed height for image and centers within frame.\n"
        "-frameWidth Sets a fixed width for image and centers within frame.\n"
        "-trim Trims excess transparent space from individual images offsets by frame size.\n"
        "-xml \'path\' Generates an SpriteSheet xml file at path.\n"
        "-debug Draws allocation boxes on sprite.\n");
}

int main(int argc, char** argv)
{
    QStringList arguments;

#ifdef WIN32
    arguments = ParseArguments(GetCommandLine());
#else
    arguments = ParseArguments(argc, argv);
#endif

    Run(arguments);
    return 0;
}

void Run(QStringList &arguments)
{
    if (arguments.size() < 2)
        Help();

    Context context;
    context.m_FileSystem.reset(new FileSystem(&context));
    context.m_LogSystem.reset(new Log(&context));
    auto* fileSystem = context.m_FileSystem.get();

    QStringList inputFiles;
    QString outputFile;
    QString spriteSheetFileName;
    bool debug = false;
    unsigned padX = 0;
    unsigned padY = 0;
    unsigned offsetX = 0;
    unsigned offsetY = 0;
    unsigned frameWidth = 0;
    unsigned frameHeight = 0;
    bool help = false;
    bool trim = false;

    while (arguments.size() > 0)
    {
        QString arg = arguments[0];
        arguments.pop_front();

        if (arg.isEmpty())
            continue;

        if (arg.startsWith("-"))
        {
            if (arg == "-px")      { padX = arguments.takeFirst().toUInt(); }
            else if (arg == "-py") { padY = arguments.takeFirst().toUInt(); }
            else if (arg == "-ox") { offsetX = arguments.takeFirst().toUInt(); }
            else if (arg == "-oy") { offsetY = arguments.takeFirst().toUInt(); }
            else if (arg == "-frameWidth") { frameWidth = arguments.takeFirst().toUInt(); }
            else if (arg == "-frameHeight") { frameHeight = arguments.takeFirst().toUInt(); }
            else if (arg == "-trim") { trim = true; }
            else if (arg == "-xml")  { spriteSheetFileName = arguments.takeFirst(); }
            else if (arg == "-h")  { help = true; break; }
            else if (arg == "-debug")  { debug = true; }
        }
        else
            inputFiles.push_back(arg);
    }

    if (help)
        Help();

    if (inputFiles.size() < 2)
        ErrorExit("An input and output file must be specified.");

    if (frameWidth ^ frameHeight)
        ErrorExit("Both frameHeight and frameWidth must be omitted or specified.");

    // take last input file as output
    if (inputFiles.size() > 1)
    {
        outputFile = inputFiles[inputFiles.size() - 1];
        URHO3D_LOGINFO("Output file set to " + outputFile + ".");
        inputFiles.pop_back();
    }

    // set spritesheet name to outputfile.xml if not specified
    if (spriteSheetFileName.isEmpty())
        spriteSheetFileName = ReplaceExtension(outputFile, ".xml");

    if (GetParentPath(spriteSheetFileName) != GetParentPath(outputFile))
        ErrorExit("Both output xml and png must be in the same folder");

    // check all input files exist
    for (unsigned i = 0; i < inputFiles.size(); ++i)
    {
        URHO3D_LOGINFO("Checking " + inputFiles[i] + " to see if file exists.");
        if (!fileSystem->FileExists(inputFiles[i]))
            ErrorExit("File " + inputFiles[i] + " does not exist.");
    }

    // Set the max offset equal to padding to prevent images from going out of bounds
    offsetX = Min((int)offsetX, (int)padX);
    offsetY = Min((int)offsetY, (int)padY);

    std::vector<SharedPtr<PackerInfo > > packerInfos;

    for (unsigned i = 0; i < inputFiles.size(); ++i)
    {
        QString path = inputFiles[i];
        QString name = ReplaceExtension(GetFileName(path), "");
        File file(&context, path);
        Image image(&context);

        if (!image.Load(file))
            ErrorExit("Could not load image " + path + ".");

        if (image.IsCompressed())
            ErrorExit(path + " is compressed. Compressed images are not allowed.");

        SharedPtr<PackerInfo> packerInfo(new PackerInfo(path, name));
        int imageWidth = image.GetWidth();
        int imageHeight = image.GetHeight();
        int trimOffsetX = 0;
        int trimOffsetY = 0;
        int adjustedWidth = imageWidth;
        int adjustedHeight = imageHeight;

        if (trim)
        {
            int minX = imageWidth;
            int minY = imageHeight;
            int maxX = 0;
            int maxY = 0;

            for (int y = 0; y < imageHeight; ++y)
            {
                for (int x = 0; x < imageWidth; ++x)
                {
                    bool found = (image.GetPixelInt(x, y) & 0x000000ffu) != 0;
                    if (found) {
                        minX = Min(minX, x);
                        minY = Min(minY, y);
                        maxX = Max(maxX, x);
                        maxY = Max(maxY, y);
                    }
                }
            }

            trimOffsetX = minX;
            trimOffsetY = minY;
            adjustedWidth = maxX - minX + 1;
            adjustedHeight = maxY - minY + 1;
        }

        if (trim)
        {
            packerInfo->frameWidth = imageWidth;
            packerInfo->frameHeight = imageHeight;
        }
        else if (frameWidth || frameHeight)
        {
            packerInfo->frameWidth = frameWidth;
            packerInfo->frameHeight = frameHeight;
        }
        packerInfo->width = adjustedWidth;
        packerInfo->height = adjustedHeight;
        packerInfo->offsetX -= trimOffsetX;
        packerInfo->offsetY -= trimOffsetY;
        packerInfos.push_back(packerInfo);
    }

    int packedWidth = MAX_TEXTURE_SIZE;
    int packedHeight = MAX_TEXTURE_SIZE;
    {
        // fill up an list of tries in increasing size and take the first win
        std::deque<IntVector2> tries;
        for(unsigned x=2; x<11; ++x)
        {
            for(unsigned y=2; y<11; ++y)
                tries.push_back(IntVector2((1u<<x), (1u<<y)));
        }

        // load rectangles
        auto* packerRects = new stbrp_rect[packerInfos.size()];
        for (unsigned i = 0; i < packerInfos.size(); ++i)
        {
            PackerInfo* packerInfo = packerInfos[i];
            stbrp_rect* packerRect = &packerRects[i];
            packerRect->id = i;
            packerRect->h = packerInfo->height + padY;
            packerRect->w = packerInfo->width + padX;
        }

        bool success = false;
        while (tries.size() > 0)
        {
            IntVector2 size = tries.front();
            tries.pop_front();
            bool fit = true;
            int textureHeight = size.y_;
            int textureWidth = size.x_;
            if (success && textureHeight * textureWidth > packedWidth * packedHeight)
                continue;

            stbrp_context packerContext;
            stbrp_node packerMemory[PACKER_NUM_NODES];
            stbrp_init_target(&packerContext, textureWidth, textureHeight, packerMemory, packerInfos.size());
            stbrp_pack_rects(&packerContext, packerRects, packerInfos.size());

            // check to see if everything fit
            for (unsigned i = 0; i < packerInfos.size(); ++i)
            {
                stbrp_rect* packerRect = &packerRects[i];
                if (!packerRect->was_packed)
                {
                    fit = false;
                    break;
                }
            }
            if (fit)
            {
                success = true;
                // distribute values to packer info
                for (unsigned i = 0; i < packerInfos.size(); ++i)
                {
                    stbrp_rect* packerRect = &packerRects[i];
                    PackerInfo* packerInfo = packerInfos[packerRect->id];
                    packerInfo->x = packerRect->x;
                    packerInfo->y = packerRect->y;
                }
                packedWidth = size.x_;
                packedHeight = size.y_;
            }
        }
        delete[] packerRects;
        if (!success)
            ErrorExit("Could not allocate for all images.  The max sprite sheet texture size is " + QString(MAX_TEXTURE_SIZE) + "x" + QString(MAX_TEXTURE_SIZE) + ".");
    }

    // create image for spritesheet
    Image spriteSheetImage(&context);
    spriteSheetImage.SetSize(packedWidth, packedHeight, 4);

    // zero out image
    spriteSheetImage.SetData(nullptr);

    XMLFile xml(&context);
    XMLElement root = xml.CreateRoot("TextureAtlas");
    root.SetAttribute("imagePath", GetFileNameAndExtension(outputFile));

    for (unsigned i = 0; i < packerInfos.size(); ++i)
    {
        SharedPtr<PackerInfo> packerInfo = packerInfos[i];
        XMLElement subTexture = root.CreateChild("SubTexture");
        subTexture.SetString("name", packerInfo->name);
        subTexture.SetInt("x", packerInfo->x + offsetX);
        subTexture.SetInt("y", packerInfo->y + offsetY);
        subTexture.SetInt("width", packerInfo->width);
        subTexture.SetInt("height", packerInfo->height);

        if (packerInfo->frameWidth || packerInfo->frameHeight)
        {
            subTexture.SetInt("frameWidth", packerInfo->frameWidth);
            subTexture.SetInt("frameHeight", packerInfo->frameHeight);
            subTexture.SetInt("offsetX", packerInfo->offsetX);
            subTexture.SetInt("offsetY", packerInfo->offsetY);
        }

        URHO3D_LOGINFO("Transferring " + packerInfo->path + " to sprite sheet.");

        File file(&context, packerInfo->path);
        Image image(&context);
        if (!image.Load(file))
            ErrorExit("Could not load image " + packerInfo->path + ".");

        for (int y = 0; y < packerInfo->height; ++y)
        {
            for (int x = 0; x < packerInfo->width; ++x)
            {
                unsigned color = image.GetPixelInt(x - packerInfo->offsetX, y - packerInfo->offsetY);
                spriteSheetImage.SetPixelInt(
                    packerInfo->x + offsetX + x,
                    packerInfo->y + offsetY + y, color);
            }
        }
    }

    if (debug)
    {
        unsigned OUTER_BOUNDS_DEBUG_COLOR = Color::BLUE.ToUInt();
        unsigned INNER_BOUNDS_DEBUG_COLOR = Color::GREEN.ToUInt();

        URHO3D_LOGINFO("Drawing debug information.");
        for (unsigned i = 0; i < packerInfos.size(); ++i)
        {
            SharedPtr<PackerInfo> packerInfo = packerInfos[i];

            // Draw outer bounds
            for (int x = 0; x < packerInfo->frameWidth; ++x)
            {
                spriteSheetImage.SetPixelInt(packerInfo->x + x, packerInfo->y, OUTER_BOUNDS_DEBUG_COLOR);
                spriteSheetImage.SetPixelInt(packerInfo->x + x, packerInfo->y + packerInfo->frameHeight, OUTER_BOUNDS_DEBUG_COLOR);
            }
            for (int y = 0; y < packerInfo->frameHeight; ++y)
            {
                spriteSheetImage.SetPixelInt(packerInfo->x, packerInfo->y + y, OUTER_BOUNDS_DEBUG_COLOR);
                spriteSheetImage.SetPixelInt(packerInfo->x + packerInfo->frameWidth, packerInfo->y + y, OUTER_BOUNDS_DEBUG_COLOR);
            }

            // Draw inner bounds
            for (int x = 0; x < packerInfo->width; ++x)
            {
                spriteSheetImage.SetPixelInt(packerInfo->x + offsetX + x, packerInfo->y + offsetY, INNER_BOUNDS_DEBUG_COLOR);
                spriteSheetImage.SetPixelInt(packerInfo->x + offsetX + x, packerInfo->y + offsetY + packerInfo->height, INNER_BOUNDS_DEBUG_COLOR);
            }
            for (int y = 0; y < packerInfo->height; ++y)
            {
                spriteSheetImage.SetPixelInt(packerInfo->x + offsetX, packerInfo->y + offsetY + y, INNER_BOUNDS_DEBUG_COLOR);
                spriteSheetImage.SetPixelInt(packerInfo->x + offsetX + packerInfo->width, packerInfo->y + offsetY + y, INNER_BOUNDS_DEBUG_COLOR);
            }
        }
    }

    URHO3D_LOGINFO("Saving output image.");
    spriteSheetImage.SavePNG(outputFile);

    URHO3D_LOGINFO("Saving SpriteSheet xml file.");
    File spriteSheetFile(&context);
    spriteSheetFile.Open(spriteSheetFileName, FILE_WRITE);
    xml.Save(spriteSheetFile);
}
