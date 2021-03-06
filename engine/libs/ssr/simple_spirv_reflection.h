/* date = November 6th 2021 9:17 pm */

#ifndef SIMPLE_SPRIV_REFLECTION_H
#define SIMPLE_SPRIV_REFLECTION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t SsrSpirvWord;

typedef enum SsrShaderType
{
    SSR_SHADER_TYPE_VERTEX,
    SSR_SHADER_TYPE_FRAGMENT,
    SSR_SHADER_TYPE_COMPUTE,
} SsrShaderType;

typedef enum SsrType
{
    SSR_TYPE_BOOL,
    SSR_TYPE_SCALAR,
    SSR_TYPE_VECTOR,
    SSR_TYPE_MATRIX,
    SSR_TYPE_STRUCT,
    SSR_TYPE_SAMPLER,
    SSR_TYPE_IMAGE,
    SSR_TYPE_ARRAY,
} SsrType;

typedef enum SsrImageDim
{
    SSR_IMAGE_1D,
    SSR_IMAGE_2D,
    SSR_IMAGE_3D,
    SSR_IMAGE_CUBE,
    SSR_IMAGE_RECT,
    SSR_IMAGE_BUFFER,
    SSR_IMAGE_SUBPASS_DATA,
} SsrImageDim;

typedef enum SsrImageDepthParameters
{
    SSR_IMAGE_DEPTH,
    SSR_IMAGE_NOT_DEPTH,
    SSR_IMAGE_DEPTH_PARAM_UNKNOWN,
} SsrImageDepthParameters;

typedef enum SsrImageSampleParameters
{
    SSR_IMAGE_USED_WITH_SAMPLER,
    SSR_IMAGE_NOT_USED_WITH_SAMPLER,
    SSR_IMAGE_SAMPLER_PARAM_UNKNOWN,
} SsrImageSampleParameters;

typedef enum SsrArraySizeKind
{
    SSR_ARRAY_SIZE_CONSTANT,
    SSR_ARRAY_SIZE_RUNTIME,
    SSR_ARRAY_SIZE_SPECIALIZATION_CONSTANT_BASED,
} SsrArraySizeKind;

typedef struct SsrTypeInfo
{
    SsrType type;
    struct SsrTypeInfo* next;
    union
    {
        struct
        {
            int dummy;  // Nothing ??
        } boolean;
        struct
        {
            size_t bitWidth;
            bool isInteger;
            bool isSigned;
        } scalar;
        struct
        {
            struct SsrTypeInfo* componentType;
            size_t numComponents;
        } vector;
        struct
        {
            struct SsrTypeInfo* columnType;
            size_t numColumns;
        } matrix;
        struct
        {
            const char* typeName;
            struct SsrTypeInfo** members;
            const char** memberNames;
            size_t numMembers;
        } structure;
        struct
        {
            int dummy; // Nothing ??
        } sampler;
        struct
        {
            struct SsrTypeInfo* sampledType;
            SsrImageDim dim;
            SsrImageDepthParameters depthParameters;
            SsrImageSampleParameters sampleParameters;
            bool isArrayed;
            bool isSampled;
            bool isMultisampled;
        } image;
        struct
        {
            struct SsrTypeInfo* entryType;
            SsrArraySizeKind sizeKind;
            size_t size;
        } array;
    } info;
} SsrTypeInfo;

typedef struct SsrShaderIO
{
    const char* name;
    SsrTypeInfo* type;
    uint32_t location;
    bool isBuiltIn;
} SsrShaderIO;

typedef enum SsrUniformKind
{
    SSR_UNIFORM_SAMPLER,
    SSR_UNIFORM_SAMPLED_IMAGE,
    SSR_UNIFORM_STORAGE_IMAGE,
    SSR_UNIFORM_COMBINED_IMAGE_SAMPLER,
    SSR_UNIFORM_UNIFORM_TEXEL_BUFFER,
    SSR_UNIFORM_STORAGE_TEXEL_BUFFER,
    SSR_UNIFORM_UNIFORM_BUFFER,
    SSR_UNIFORM_STORAGE_BUFFER,
    SSR_UNIFORM_INPUT_ATTACHMENT,
    SSR_UNIFORM_ACCELERATION_STRUCTURE,
} SsrUniformKind;

typedef struct SsrUniform
{
    const char* name;
    SsrTypeInfo* type;
    SsrUniformKind kind;
    uint32_t set;
    uint32_t binding;
    uint32_t inputAttachmentIndex;
} SsrUniform;

typedef struct SsrSpecializationConstant
{
    const char* name;
    SsrTypeInfo* type;
    uint32_t constantId;
    union
    {
        uint32_t asUint;
        int32_t asInt;
        float asFloat;
        bool asBool;
    };
} SsrSpecializationConstant;

typedef struct SsrAllocator
{
    void* userData;
    void* (*alloc)(void* userData, size_t size);
    void  (*free)(void* userData, void* ptr, size_t size);
} SsrAllocator;

typedef struct SsrMemoryBlock
{
    void* base;
    size_t capacity;
    size_t size;
    struct SsrMemoryBlock* next;
} SsrMemoryBlock;

typedef struct SimpleSpirvReflection
{
    SsrAllocator allocator;

    SsrShaderType shaderType;
    const char* entryPointName;

    SsrTypeInfo* typeInfos;

    SsrTypeInfo* pushConstantType;
    const char* pushConstantName;

    SsrShaderIO* inputs;
    size_t numInputs;

    SsrShaderIO* outputs;
    size_t numOutputs;

    SsrUniform* uniforms;
    size_t numUniforms;

    SsrSpecializationConstant* specializationConstants;
    size_t numSpecializationConstants;

    uint32_t computeWorkGroupSizeX;
    uint32_t computeWorkGroupSizeY;
    uint32_t computeWorkGroupSizeZ;

    SsrMemoryBlock* memory;

} SimpleSpirvReflection;

typedef struct SsrCreateInfo
{
    const SsrAllocator* persistentAllocator;
    const SsrAllocator* nonPersistentAllocator;
    const SsrSpirvWord* bytecode;
    size_t bytecodeNumWords;
} SsrCreateInfo;

void ssr_construct(SimpleSpirvReflection* reflection, const SsrCreateInfo* createInfo);
void ssr_destroy(SimpleSpirvReflection* reflection);

size_t ssr_get_type_size(const SsrTypeInfo* typeInfo);
const char* ssr_shader_type_to_str(SsrShaderType shader);
const char* ssr_type_to_str(SsrType type);
const char* ssr_uniform_kind_to_str(SsrUniformKind kind);

#define ssr_is_bool(typePtr)    (typePtr->type == SSR_TYPE_BOOL)
#define ssr_is_int(typePtr)     ((typePtr->type == SSR_TYPE_SCALAR) && typePtr->info.scalar.isInteger && typePtr->info.scalar.isSigned)
#define ssr_is_uint(typePtr)    ((typePtr->type == SSR_TYPE_SCALAR) && typePtr->info.scalar.isInteger && !typePtr->info.scalar.isSigned)
#define ssr_is_float(typePtr)   ((typePtr->type == SSR_TYPE_SCALAR) && !typePtr->info.scalar.isInteger)
#define ssr_is_vector(typePtr)  (typePtr->type == SSR_TYPE_VECTOR)
#define ssr_is_matrix(typePtr)  (typePtr->type == SSR_TYPE_MATRIX)
#define ssr_is_struct(typePtr)  (typePtr->type == SSR_TYPE_STRUCT)
#define ssr_is_sampler(typePtr) (typePtr->type == SSR_TYPE_SAMPLER)
#define ssr_is_image(typePtr)   (typePtr->type == SSR_TYPE_IMAGE)
#define ssr_is_array(typePtr)   (typePtr->type == SSR_TYPE_ARRAY)

#define ssr_get_vector_components_num(typePtr)      (typePtr->info.vector.numComponents)
#define ssr_get_vector_component_type(typePtr)      (typePtr->info.vector.componentType)
#define ssr_get_matrix_columns_num(typePtr)         (typePtr->info.matrix.numColumns)
#define ssr_get_matrix_rows_num(typePtr)            (ssr_get_vector_components_num(typePtr->info.matrix.columnType))
#define ssr_get_matrix_columns_type(typePtr)        (typePtr->info.matrix.columnType)
#define ssr_get_struct_type_name(typePtr)           (typePtr->info.structure.typeName)
#define ssr_get_struct_members_num(typePtr)         (typePtr->info.structure.numMembers)
#define ssr_get_struct_member_type(typePtr, id)     (typePtr->info.structure.members[id])
#define ssr_get_struct_member_name(typePtr, id)     (typePtr->info.structure.memberNames[id])
#define ssr_get_image_dim(typePtr)                  (typePtr->info.image.dim)
#define ssr_get_image_depth_parameters(typePtr)     (typePtr->info.image.depthParameters)
#define ssr_get_image_sample_parameters(typePtr)    (typePtr->info.image.sampleParameters)
#define ssr_is_image_arrayed(typePtr)               (typePtr->info.image.isArrayed)
#define ssr_is_image_sampled(typePtr)               (typePtr->info.image.isSampled)
#define ssr_is_image_multisampled(typePtr)          (typePtr->info.image.isMultisampled)
#define ssr_get_array_entry_type(typePtr)           (typePtr->info.array.entryType)
#define ssr_get_array_size_kind(typePtr)            (typePtr->info.array.sizeKind)
#define ssr_get_array_size(typePtr)                 (typePtr->info.array.size)

#ifdef __cplusplus
}
#endif

#endif //SIMPLE_SPRIV_REFLECTION_H

#ifdef SSR_IMPL
#ifndef SSR_IMPL_INNER
#define SSR_IMPL_INNER

#ifdef __cplusplus
extern "C" {
#endif

#include <spirv-headers/spirv.h>

#ifndef ssr_printf
#   include <stdio.h>
#   define ssr_printf(fmt, ...) printf(fmt, __VA_ARGS__)
#endif

#ifndef ssr_assert
#   include <assert.h>
#   define ssr_assert(condition) assert(condition)
#endif

#ifndef ssr_memset
#   include <string.h>
#   define ssr_memset(ptr, val, size) memset(ptr, val, size)
#endif

#ifndef ssr_strlen
#   include <string.h>
#   define ssr_strlen(str) strlen(str)
#endif

#ifndef ssr_memcpy
#   include <string.h>
#   define ssr_memcpy(dst, src, size) memcpy(dst, src, size)
#endif

#ifndef ssr_strcmp
#   include <string.h>
#   define ssr_strcmp(first, second) (strcmp(first, second) == 0)
#endif

#ifndef ssr_array_size
#   define ssr_array_size(array) (sizeof(array) / sizeof(array[0]))
#endif

#ifndef ssr_indent
#   define ssr_indent "    "
#endif

#ifdef SSR_DIRTY_ALLOCATOR
#   define ssr_clear_mem(mem, size) ssr_memset(mem, 0, size)
#else
#   define ssr_clear_mem(mem, size)
#endif

#ifndef SSR_DEFAULT_MEMORY_BLOCK_SIZE
#   define SSR_DEFAULT_MEMORY_BLOCK_SIZE 4096
#endif

#define ssr_opcode(word) ((uint16_t)word)

typedef enum SsrDecoration
{
    SSR_DECORATION_BLOCK        = 0x00000001,
    SSR_DECORATION_BUFFER_BLOCK = 0x00000002,
    SSR_DECORATION_OFFSET       = 0x00000004,
    SSR_DECORATION_INPUT_ATTACHMENT_INDEX = 0x00000008,
    SSR_DECORATION_LOCATION     = 0x00000010,
    SSR_DECORATION_BINDING      = 0x00000020,
    SSR_DECORATION_SET          = 0x00000040,
    SSR_DECORATION_BUILT_IN     = 0x00000080,
    SSR_DECORATION_SPEC_ID      = 0x00000100,
} SsrDecoration;

typedef struct SsrSpirvId
{
    const SsrSpirvWord* declarationLocation;
    const char* name;
    SsrSpirvWord storageClass;
    uint32_t decorations;
    uint32_t location;
    uint32_t binding;
    uint32_t set;
    uint32_t specId;
    uint32_t inputAttachmentIndex;
} SsrSpirvId;

typedef struct SsrSpirvStructMember
{
    SsrSpirvId* id;
    const char* name;
} SsrSpirvStructMember;

typedef struct SsrSpirvStruct
{
    SsrSpirvId* id;
    SsrSpirvStructMember* members;
    size_t numMembers;
} SsrSpirvStruct;

SsrMemoryBlock* __ssr_alloc_single_block(size_t size, SsrAllocator* allocator)
{
    /*
        By default memory is allocated in 4KB chunks.
        If requested size is bigger, system will allocate memory of that size instead of default chunk size.
    */
    const size_t DEFAULT_BLOCK_SIZE = SSR_DEFAULT_MEMORY_BLOCK_SIZE - sizeof(SsrMemoryBlock); // Default memory size that is available to user in each memory block
    const size_t memoryBlockCapacity = (size <= DEFAULT_BLOCK_SIZE ? DEFAULT_BLOCK_SIZE : size);
    const size_t allocationSize = sizeof(SsrMemoryBlock) + memoryBlockCapacity;
    SsrMemoryBlock* block = (SsrMemoryBlock*)allocator->alloc(allocator->userData, allocationSize);
    block->base = ((char*)block) + sizeof(SsrMemoryBlock);
    block->capacity = memoryBlockCapacity;
    block->size = 0;
    block->next = NULL;
    return block;
}

void* __ssr_alloc(size_t allocationSize, SimpleSpirvReflection* reflection)
{
    SsrMemoryBlock* block = reflection->memory;
    SsrMemoryBlock* prevBlock = NULL;
    for (; block; block = block->next)
    {
        if ((block->capacity - block->size) >= allocationSize)
        {
            break;
        }
        prevBlock = block;
    }
    if (!block)
    {
        block = __ssr_alloc_single_block(allocationSize, &reflection->allocator);
        if (prevBlock)
            prevBlock->next = block;
        else
            reflection->memory = block;
    }
    void* result = ((char*)block->base) + block->size;
    block->size += allocationSize;
    return result;
}

char* __ssr_save_string(const char* str, SimpleSpirvReflection* reflection)
{
    if (!str)
    {
        return (char*)"";
    }
    size_t length = ssr_strlen(str);
    char* result = (char*)__ssr_alloc(length + 1, reflection);
    ssr_memcpy(result, str, length);
    result[length] = 0;
    return result;
}

SsrTypeInfo* __ssr_save_or_get_type_from_reflection(SsrSpirvStruct* structs, size_t numStructs, SsrSpirvId* ids, SsrSpirvId* id, SimpleSpirvReflection* reflection)
{
    SsrTypeInfo* matchedType = NULL;
    SsrTypeInfo resultType = {0};
    //
    // 1. Get type kind
    //
    uint16_t spvType = ssr_opcode(id->declarationLocation[0]);
    switch (spvType)
    {
        case SpvOpTypeBool:         resultType.type = SSR_TYPE_BOOL; break;
        case SpvOpTypeFloat:        resultType.type = SSR_TYPE_SCALAR; break;
        case SpvOpTypeInt:          resultType.type = SSR_TYPE_SCALAR; break;
        case SpvOpTypeVector:       resultType.type = SSR_TYPE_VECTOR; break;
        case SpvOpTypeMatrix:       resultType.type = SSR_TYPE_MATRIX; break;
        case SpvOpTypeStruct:       resultType.type = SSR_TYPE_STRUCT; break;
        case SpvOpTypeSampler:      resultType.type = SSR_TYPE_SAMPLER; break;
        case SpvOpTypeSampledImage: resultType.type = SSR_TYPE_IMAGE; break;
        case SpvOpTypeImage:        resultType.type = SSR_TYPE_IMAGE; break;
        case SpvOpTypeRuntimeArray: resultType.type = SSR_TYPE_ARRAY; break;
        case SpvOpTypeArray:        resultType.type = SSR_TYPE_ARRAY; break;
        default: { ssr_assert(!"Unsupported OpType"); }
    }
    //
    // 2. Process result type based on it's kind
    //
    switch(resultType.type)
    {
        case SSR_TYPE_BOOL:
        {
            //
            // Try match bool
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_BOOL)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_SCALAR:
        {
            //
            // Scalar types can be either floats or doubles
            //
            switch(spvType)
            {
                case SpvOpTypeFloat:
                {
                    resultType.info.scalar.isInteger = false;
                    resultType.info.scalar.isSigned = true;
                    resultType.info.scalar.bitWidth = id->declarationLocation[2];
                } break;
                case SpvOpTypeInt:
                {
                    resultType.info.scalar.isInteger = true;
                    resultType.info.scalar.isSigned = id->declarationLocation[3] == 1;
                    resultType.info.scalar.bitWidth = id->declarationLocation[2];
                } break;
                default: { ssr_assert(!"Unsupported scalar value type (bool?)"); }
            }
            //
            // Try to find scalar type with same properties in type list
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_SCALAR &&
                    alreadySavedValue->info.scalar.isInteger == resultType.info.scalar.isInteger &&
                    alreadySavedValue->info.scalar.bitWidth == resultType.info.scalar.bitWidth &&
                    alreadySavedValue->info.scalar.isSigned == resultType.info.scalar.isSigned)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_VECTOR:
        {
            //
            // Save vector info
            //
            resultType.info.vector.numComponents = id->declarationLocation[3];
            resultType.info.vector.componentType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, &ids[id->declarationLocation[2]], reflection);
            ssr_assert(resultType.info.vector.componentType->type == SSR_TYPE_SCALAR);
            //
            // Try to find vector type with same properties in type list
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_VECTOR &&
                    alreadySavedValue->info.vector.numComponents == resultType.info.vector.numComponents &&
                    alreadySavedValue->info.vector.componentType == resultType.info.vector.componentType)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_MATRIX:
        {
            //
            // Save matrix info
            //
            resultType.info.matrix.numColumns = id->declarationLocation[3];
            resultType.info.matrix.columnType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, &ids[id->declarationLocation[2]], reflection);
            ssr_assert(resultType.info.matrix.columnType->type == SSR_TYPE_VECTOR);
            //
            // Try to find matrix type with same properties in type list
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_MATRIX &&
                    alreadySavedValue->info.matrix.numColumns == resultType.info.matrix.numColumns &&
                    alreadySavedValue->info.matrix.columnType == resultType.info.matrix.columnType)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_STRUCT:
        {
            //
            // Find corresponding SsrSpirvStruct
            //
            SsrSpirvStruct* spirvStruct = NULL;
            for (size_t structIt = 0; structIt < numStructs; structIt++)
            {
                if (structs[structIt].id == id)
                {
                    spirvStruct= &structs[structIt];
                    break;
                }
            }
            ssr_assert(spirvStruct);
            resultType.info.structure.typeName = spirvStruct->id->name;
            //
            // Try match struct
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_STRUCT &&
                    ssr_strcmp(alreadySavedValue->info.structure.typeName, spirvStruct->id->name))
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
            //
            // Save struct name and members if not matched
            //
            if (!matchedType)
            {
                resultType.info.structure.typeName = __ssr_save_string(spirvStruct->id->name, reflection);
                resultType.info.structure.numMembers = spirvStruct->numMembers;
                resultType.info.structure.members = (SsrTypeInfo**)__ssr_alloc(sizeof(SsrTypeInfo*) * resultType.info.structure.numMembers, reflection);
                resultType.info.structure.memberNames = (const char**)__ssr_alloc(sizeof(const char*) * resultType.info.structure.numMembers, reflection);
                for (size_t memberIt = 0; memberIt < resultType.info.structure.numMembers; memberIt++)
                {
                    resultType.info.structure.members[memberIt] = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, spirvStruct->members[memberIt].id, reflection);
                    resultType.info.structure.memberNames[memberIt] = __ssr_save_string(spirvStruct->members[memberIt].name, reflection);
                }
            }
        } break;
        case SSR_TYPE_SAMPLER:
        {
            //
            // Try match sampler
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_SAMPLER)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_IMAGE:
        {
            ssr_assert((spvType == SpvOpTypeSampledImage || spvType == SpvOpTypeImage) && "Unknown image type");
            //
            // Go to actual OpTypeImage and set isSampled param
            //
            SsrSpirvId* imageId = id;
            if (spvType == SpvOpTypeSampledImage)
            {
                resultType.info.image.isSampled = true;
                // OpTypeSampledImage just references OpTypeImage:
                // https://www.khronos.org/registry/SPIR-V/specs/unified1/SPIRV.html#OpTypeSampledImage
                imageId = &ids[id->declarationLocation[2]];
            }
            else if (spvType == SpvOpTypeImage)
            {
                resultType.info.image.isSampled = false;
            }
            else
            {
                ssr_assert(!"Unknown image type");
            }
            ssr_assert(ssr_opcode(imageId->declarationLocation[0]) == SpvOpTypeImage);
            //
            // Retrieve image params
            //
            const SpvDim imageDim = (SpvDim)imageId->declarationLocation[3];
            const SsrSpirvWord imageDepth = imageId->declarationLocation[4];
            const SsrSpirvWord isArrayed = imageId->declarationLocation[5];
            const SsrSpirvWord isMultisampled = imageId->declarationLocation[6];
            const SsrSpirvWord imageSampledInfo = imageId->declarationLocation[7];
            const SpvImageFormat imageFormat = (SpvImageFormat)imageId->declarationLocation[8];
            //
            // Get sampled type (can be void)
            //
            SsrSpirvId* sampledTypeId = &ids[imageId->declarationLocation[2]];
            if (sampledTypeId->declarationLocation[0] == SpvOpTypeVoid)
            {
                resultType.info.image.sampledType = NULL;
            }
            else
            {
                resultType.info.image.sampledType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, sampledTypeId, reflection);
            }
            //
            // Convert image params to ssr's representation
            //
            switch (imageDim)
            {
                case SpvDim1D: resultType.info.image.dim = SSR_IMAGE_1D; break;
                case SpvDim2D: resultType.info.image.dim = SSR_IMAGE_2D; break;
                case SpvDim3D: resultType.info.image.dim = SSR_IMAGE_3D; break;
                case SpvDimCube: resultType.info.image.dim = SSR_IMAGE_CUBE; break;
                case SpvDimRect: resultType.info.image.dim = SSR_IMAGE_RECT; break;
                case SpvDimBuffer: resultType.info.image.dim = SSR_IMAGE_BUFFER; break;
                case SpvDimSubpassData: resultType.info.image.dim = SSR_IMAGE_SUBPASS_DATA; break;
                default: { ssr_assert(!"Unknown SpvDim value"); }
            }
            switch (imageDepth)
            {
                case 0: resultType.info.image.depthParameters = SSR_IMAGE_NOT_DEPTH; break;
                case 1: resultType.info.image.depthParameters = SSR_IMAGE_DEPTH; break;
                case 2: resultType.info.image.depthParameters = SSR_IMAGE_DEPTH_PARAM_UNKNOWN; break;
                default: { ssr_assert(!"Unknown OpTypeImage depth value"); }
            }
            switch (imageSampledInfo)
            {
                case 0: resultType.info.image.sampleParameters = SSR_IMAGE_SAMPLER_PARAM_UNKNOWN; break;
                case 1: resultType.info.image.sampleParameters = SSR_IMAGE_USED_WITH_SAMPLER; break;
                case 2: resultType.info.image.sampleParameters = SSR_IMAGE_NOT_USED_WITH_SAMPLER; break;
                default: { ssr_assert(!"Unknown OpTypeImage sample value"); }
            }
            resultType.info.image.isArrayed = isArrayed != 0;
            resultType.info.image.isMultisampled = isMultisampled != 0;
            //
            // Try match image
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_IMAGE &&
                    alreadySavedValue->info.image.sampledType == resultType.info.image.sampledType &&
                    alreadySavedValue->info.image.isSampled == resultType.info.image.isSampled &&
                    alreadySavedValue->info.image.dim == resultType.info.image.dim &&
                    alreadySavedValue->info.image.depthParameters == resultType.info.image.depthParameters &&
                    alreadySavedValue->info.image.sampleParameters == resultType.info.image.sampleParameters &&
                    alreadySavedValue->info.image.isArrayed == resultType.info.image.isArrayed &&
                    alreadySavedValue->info.image.isMultisampled == resultType.info.image.isMultisampled)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        case SSR_TYPE_ARRAY:
        {
            //
            // Retrieve array data
            //
            switch (spvType)
            {
                case SpvOpTypeRuntimeArray:
                {
                    resultType.info.array.sizeKind = SSR_ARRAY_SIZE_RUNTIME;
                    resultType.info.array.size = 0;
                } break;
                case SpvOpTypeArray:
                {
                    SsrSpirvId* arrayLength = &ids[id->declarationLocation[3]];
                    if (ssr_opcode(arrayLength->declarationLocation[0]) == SpvOpSpecConstantOp)
                    {
                        resultType.info.array.sizeKind = SSR_ARRAY_SIZE_SPECIALIZATION_CONSTANT_BASED;
                        resultType.info.array.size = 0;
                    }
                    else
                    {
                        // Go to OpConstant describing array length
                        ssr_assert(ssr_opcode(arrayLength->declarationLocation[0]) == SpvOpConstant);
                        resultType.info.array.sizeKind = SSR_ARRAY_SIZE_CONSTANT;
                        // Check that constant type is integer
                        SsrSpirvId* arrayLengthType = &ids[arrayLength->declarationLocation[1]];
                        ssr_assert(ssr_opcode(arrayLengthType->declarationLocation[0]) == SpvOpTypeInt);
                        // Check constant bit width and save array size
                        SsrSpirvWord arrayLengthTypeBitWidth = arrayLengthType->declarationLocation[2];
                        switch (arrayLengthTypeBitWidth)
                        {
                            case 32: resultType.info.array.size = *((uint32_t*)&arrayLength->declarationLocation[3]); break;
                            default: { ssr_assert(!"Unsupported array size constat bit width"); }
                        };
                    }
                } break;
                default: { ssr_assert(!"Unknown array type"); }
            }
            resultType.info.array.entryType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, &ids[id->declarationLocation[2]], reflection);
            //
            // Try match array
            //
            SsrTypeInfo* alreadySavedValue = reflection->typeInfos;
            while (alreadySavedValue)
            {
                if (alreadySavedValue->type == SSR_TYPE_ARRAY &&
                    alreadySavedValue->info.array.sizeKind == resultType.info.array.sizeKind &&
                    alreadySavedValue->info.array.size == resultType.info.array.size &&
                    alreadySavedValue->info.array.entryType == resultType.info.array.entryType)
                {
                    matchedType = alreadySavedValue;
                    break;
                }
                alreadySavedValue = alreadySavedValue->next;
            }
        } break;
        default: { ssr_assert(!"Unsupported SSR_TYPE_* value"); }
    }
    //
    // 3. If type is not yet saved in reflection, save it
    //
    if (!matchedType)
    {
        matchedType = (SsrTypeInfo*)__ssr_alloc(sizeof(SsrTypeInfo), reflection);
        ssr_memcpy(matchedType, &resultType, sizeof(SsrTypeInfo));
        if (reflection->typeInfos)
        {
            SsrTypeInfo* lastTypeInfo = reflection->typeInfos;
            while (lastTypeInfo->next)
            {
                lastTypeInfo = lastTypeInfo->next;
            }
            lastTypeInfo->next = matchedType;
        }
        else
        {
            reflection->typeInfos = matchedType;
        }
    }
    return matchedType;
}

void __ssr_process_shader_io_variable(SsrSpirvStruct* structs, size_t numStructs, SsrSpirvId* ids, SsrSpirvId* ioDeclarationId, SimpleSpirvReflection* reflection, SsrShaderIO* io)
{
    io->name = __ssr_save_string(ioDeclarationId->name, reflection);
    io->location = ioDeclarationId->location;
    ssr_assert(ssr_opcode(ioDeclarationId->declarationLocation[0]) == SpvOpVariable);
    SsrSpirvId* id = &ids[ioDeclarationId->declarationLocation[1]];
    ssr_assert(ssr_opcode(id->declarationLocation[0]) == SpvOpTypePointer);
    id = &ids[id->declarationLocation[3]];
    io->type = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, id, reflection);
    io->isBuiltIn = (ioDeclarationId->decorations & SSR_DECORATION_BUILT_IN) || (id->decorations & SSR_DECORATION_BUILT_IN);
}

void __ssr_process_shader_uniform(SsrSpirvStruct* structs, size_t numStructs, SsrSpirvId* ids, SsrSpirvId* uniformIdOpVariable, SimpleSpirvReflection* reflection)
{
    // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#interfaces-resources-descset
    // https://github.com/KhronosGroup/Vulkan-Guide/blob/master/chapters/mapping_data_to_shaders.md
    SsrUniform* uniform = &reflection->uniforms[reflection->numUniforms++];
    //
    // Start from variable declaration
    //
    const SsrSpirvWord* opVariable = uniformIdOpVariable->declarationLocation;
    ssr_assert(ssr_opcode(opVariable[0]) == SpvOpVariable);
    ssr_assert(uniformIdOpVariable->name);
    ssr_assert(uniformIdOpVariable->decorations & SSR_DECORATION_SET);
    ssr_assert(uniformIdOpVariable->decorations & SSR_DECORATION_BINDING);
    uniform->name = __ssr_save_string(uniformIdOpVariable->name, reflection);
    uniform->set = uniformIdOpVariable->set;
    uniform->binding = uniformIdOpVariable->binding;
    if (uniformIdOpVariable->decorations & SSR_DECORATION_INPUT_ATTACHMENT_INDEX)
    {
        // @TODO : Check if this is correct
        uniform->inputAttachmentIndex = uniformIdOpVariable->inputAttachmentIndex;
    }
    //
    // Go to OpTypePointer describing this variable
    //
    const SsrSpirvWord* opTypePointer = ids[opVariable[1]].declarationLocation;
    ssr_assert(ssr_opcode(opTypePointer[0]) == SpvOpTypePointer);
    SsrSpirvId* uniformIdOpType = &ids[opTypePointer[3]];
    uniform->type = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, uniformIdOpType, reflection);
    uint16_t opType = ssr_opcode(uniformIdOpType->declarationLocation[0]);
    SpvStorageClass storageClass = (SpvStorageClass)opTypePointer[2];
    if (opType == SpvOpTypeArray)
    {
        uniformIdOpType = &ids[uniformIdOpType->declarationLocation[2]]; // go to array entry type id
        opType = ssr_opcode(uniformIdOpType->declarationLocation[0]); // get array entry OpType
    }
    if (storageClass == SpvStorageClassUniform)
    {
        if (opType == SpvOpTypeStruct)
        {
            const bool isBlock = uniformIdOpType->decorations & SSR_DECORATION_BLOCK;
            const bool isBufferBlock = uniformIdOpType->decorations & SSR_DECORATION_BUFFER_BLOCK;
            ssr_assert((isBlock && !isBufferBlock) || (isBufferBlock && !isBlock));
            if      (isBlock)       uniform->kind = SSR_UNIFORM_UNIFORM_BUFFER;
            else if (isBufferBlock) uniform->kind = SSR_UNIFORM_STORAGE_BUFFER;
        }
        else { ssr_assert(!"Uniform with StorageClassUniform has unsupported OpType"); }
    }
    else if (storageClass == SpvStorageClassUniformConstant)
    {
        if (opType == SpvOpTypeImage)
        {
            const SsrSpirvWord sampled = uniformIdOpType->declarationLocation[7];
            const SpvDim dim = (SpvDim)uniformIdOpType->declarationLocation[3];
            if (sampled == 1)
            {
                if      (dim == SpvDimBuffer)       uniform->kind = SSR_UNIFORM_UNIFORM_TEXEL_BUFFER;
                else if (dim == SpvDimSubpassData)  { ssr_assert(!"Unsupported uniform type"); }
                else                                uniform->kind = SSR_UNIFORM_SAMPLED_IMAGE;
            }
            else if (sampled == 2)
            {
                if      (dim == SpvDimBuffer)       uniform->kind = SSR_UNIFORM_STORAGE_TEXEL_BUFFER;
                else if (dim == SpvDimSubpassData)  uniform->kind = SSR_UNIFORM_INPUT_ATTACHMENT;
                else                                uniform->kind = SSR_UNIFORM_STORAGE_IMAGE;
            }
            else { ssr_assert(!"Unsupported \"Sampled\" parameter for OpTypeImage"); }
        }
        else if (opType == SpvOpTypeSampler)                    uniform->kind = SSR_UNIFORM_SAMPLER;
        else if (opType == SpvOpTypeSampledImage)               uniform->kind = SSR_UNIFORM_COMBINED_IMAGE_SAMPLER;
        else if (opType == SpvOpTypeAccelerationStructureKHR)   uniform->kind = SSR_UNIFORM_ACCELERATION_STRUCTURE;
        else { ssr_assert(!"Uniform with StorageClassUniformConstant has unsupported OpType"); }
    }
    else if (storageClass == SpvStorageClassStorageBuffer)
    {
        if (opType == SpvOpTypeStruct) uniform->kind = SSR_UNIFORM_STORAGE_BUFFER;
        else { ssr_assert(!"Uniform with StorageClassStorageBuffer has unsupported OpType"); }
    }
    else { ssr_assert(!"Unsupported uniform SpvStorageClass"); }
}

void ssr_construct(SimpleSpirvReflection* reflection, const SsrCreateInfo* createInfo)
{
#define ssr_save_decl_location(index) { const SsrSpirvWord id = instruction[index]; ssr_assert(id < bound); ids[id].declarationLocation = instruction; }

    ssr_clear_mem(reflection, sizeof(SimpleSpirvReflection));
    reflection->allocator = *createInfo->persistentAllocator;
    const SsrAllocator* nonPersistentAllocator = createInfo->nonPersistentAllocator;
    const SsrSpirvWord* bytecode = createInfo->bytecode;
    const size_t wordCount = createInfo->bytecodeNumWords;
    //
    // Basic header validation + retrieving number of unique ids (bound) used in bytecode
    //
    ssr_assert(bytecode[0] == SpvMagicNumber);
    size_t bound = bytecode[3];
    const SsrSpirvWord* instruction = NULL;
    //
    // Allocate SsrSpirvId array which will hold all neccessary information about shader identifiers
    //
    SsrSpirvId* ids = (SsrSpirvId*)nonPersistentAllocator->alloc(nonPersistentAllocator->userData, sizeof(SsrSpirvId) * bound);
    ssr_clear_mem(ids, sizeof(SsrSpirvId) * bound);
    //
    // Step 1. Filling ids array + some general shader info
    //
    size_t inputsCount = 0;
    size_t outputsCount = 0;
    size_t uniformsCount = 0;
    size_t specializationConstantsCount = 0;
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        const uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        const uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpEntryPoint:
            {
                ssr_assert(instructionWordCount >= 4);
                if (instruction[1] == SpvExecutionModelVertex)
                    reflection->shaderType = SSR_SHADER_TYPE_VERTEX;
                else if (instruction[1] == SpvExecutionModelFragment)
                    reflection->shaderType = SSR_SHADER_TYPE_FRAGMENT;
                else if (instruction[1] == SpvExecutionModelGLCompute)
                    reflection->shaderType = SSR_SHADER_TYPE_COMPUTE;
                else { ssr_assert(!"Only vertex, fragment and compute shaders are supported"); }
                reflection->entryPointName = __ssr_save_string((char*)&instruction[3], reflection);
            } break;
            case SpvOpName:
            {
                ssr_assert(instructionWordCount >= 3);
                ids[instruction[1]].name = (char*)&instruction[2];
            } break;
            case SpvOpConstant:
            {
                ssr_assert(instructionWordCount >= 4);
                ssr_save_decl_location(2);
            } break;
            case SpvOpTypeVoid:
            {
                ssr_assert(instructionWordCount == 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeBool:
            {
                ssr_assert(instructionWordCount == 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeFloat:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeInt:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeVector:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeMatrix:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeStruct:
            {
                ssr_assert(instructionWordCount >= 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeSampler:
            {
                ssr_assert(instructionWordCount == 2);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeSampledImage:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeImage:
            {
                ssr_assert(instructionWordCount >= 9);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeRuntimeArray:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypeArray:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
            } break;
            case SpvOpTypePointer:
            {
                ssr_assert(instructionWordCount == 4);
                ssr_save_decl_location(1);
                ids[instruction[1]].storageClass = instruction[2];
            } break;
            case SpvOpVariable:
            {
                ssr_assert(instructionWordCount >= 4);
                ssr_save_decl_location(2);
                const SsrSpirvWord storageClass = instruction[3];
                const SsrSpirvWord resultId = instruction[2];
                ids[resultId].storageClass = storageClass;
                if      (storageClass == SpvStorageClassInput) inputsCount += 1;
                else if (storageClass == SpvStorageClassOutput) outputsCount += 1;
                else if (storageClass == SpvStorageClassUniform) uniformsCount += 1;
                else if (storageClass == SpvStorageClassUniformConstant) uniformsCount += 1;
                else if (storageClass == SpvStorageClassStorageBuffer) uniformsCount += 1;
            } break;
            case SpvOpMemberDecorate:
            {
                ssr_assert(instructionWordCount >= 4);
                const SpvDecoration decoration = (SpvDecoration)instruction[3];
                switch (decoration)
                {
                    case SpvDecorationBuiltIn:
                    {
                        // Here we basically saying that if structure member has "BuiltIn" decoration,
                        // then the whole structure is considered "BuiltIn" and will be ignored in reflection
                        ssr_assert(instructionWordCount == 5);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BUILT_IN;
                    } break;
                }
            } break;
            case SpvOpDecorate:
            {
                const SpvDecoration decoration = (SpvDecoration)instruction[2];
                switch (decoration)
                {
                    case SpvDecorationBlock:
                    {
                        ssr_assert(instructionWordCount == 3);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BLOCK;
                    } break;
                    case SpvDecorationBufferBlock:
                    {
                        ssr_assert(instructionWordCount == 3);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BUFFER_BLOCK;
                    } break;
                    case SpvDecorationBuiltIn:
                    {
                        ssr_assert(instructionWordCount == 4);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].decorations |= SSR_DECORATION_BUILT_IN;
                    } break;
                    case SpvDecorationLocation:
                    {
                        ssr_assert(instructionWordCount == 4);
                        SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].location = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_LOCATION;
                    } break;
                    case SpvDecorationBinding:
                    {
                        ssr_assert(instructionWordCount == 4);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].binding = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_BINDING;
                    } break;
                    case SpvDecorationDescriptorSet:
                    {
                        ssr_assert(instructionWordCount == 4);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].set = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_SET;
                    } break;
                    case SpvDecorationInputAttachmentIndex:
                    {
                        ssr_assert(instructionWordCount == 4);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].inputAttachmentIndex = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_INPUT_ATTACHMENT_INDEX;
                    } break;
                    case SpvDecorationSpecId:
                    {
                        ssr_assert(instructionWordCount == 4);
                        const SsrSpirvWord id = instruction[1];
                        ssr_assert(id < bound);
                        ids[id].specId = instruction[3];
                        ids[id].decorations |= SSR_DECORATION_SPEC_ID;
                    } break;
                }
            }
            case SpvOpExecutionMode:
            {
                ssr_assert(instructionWordCount >= 3);
                const SsrSpirvWord executionMode = instruction[2];
                if (executionMode == SpvExecutionModeLocalSize)
                {
                    ssr_assert(instructionWordCount == 6);
                    reflection->computeWorkGroupSizeX = instruction[3];
                    reflection->computeWorkGroupSizeY = instruction[4];
                    reflection->computeWorkGroupSizeZ = instruction[5];
                }
            } break;
            case SpvOpSpecConstant:
            {
                ssr_assert(instructionWordCount >= 4);
                ssr_save_decl_location(2);
                specializationConstantsCount += 1;
            } break;
            case SpvOpSpecConstantFalse:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(2);
                specializationConstantsCount += 1;
            } break;
            case SpvOpSpecConstantTrue:
            {
                ssr_assert(instructionWordCount == 3);
                ssr_save_decl_location(2);
                specializationConstantsCount += 1;
            } break;
            case SpvOpSpecConstantOp:
            {
                ssr_assert(instructionWordCount >= 4);
                ssr_save_decl_location(2);
            } break;
        }
        instruction += instructionWordCount;
    }
    reflection->inputs = (SsrShaderIO*)__ssr_alloc(sizeof(SsrShaderIO) * inputsCount, reflection);
    reflection->outputs = (SsrShaderIO*)__ssr_alloc(sizeof(SsrShaderIO) * outputsCount, reflection);
    reflection->uniforms = (SsrUniform*)__ssr_alloc(sizeof(SsrUniform) * uniformsCount, reflection);
    reflection->specializationConstants = (SsrSpecializationConstant*)__ssr_alloc(sizeof(SsrSpecializationConstant) * specializationConstantsCount, reflection);
    //
    // Step 2. Retrieving information about shader structs
    //
    //
    // Step 2.1. Count number of structs and struct members
    //
    size_t numStructs = 0;
    size_t numStructMembers = 0;
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        const uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        const uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpTypeStruct:
            {
                numStructs += 1;
                numStructMembers += instructionWordCount - 2;
            } break;
        }
        instruction += instructionWordCount;
    }
    //
    // Step 2.2. Allocate structs and struct members arrays
    //
    SsrSpirvStruct* structs = (SsrSpirvStruct*)nonPersistentAllocator->alloc(nonPersistentAllocator->userData, sizeof(SsrSpirvStruct) * numStructs);
    SsrSpirvStructMember* structMembers = (SsrSpirvStructMember*)nonPersistentAllocator->alloc(nonPersistentAllocator->userData, sizeof(SsrSpirvStructMember) * numStructMembers);
    ssr_clear_mem(structs, sizeof(SsrSpirvStruct) * numStructs);
    ssr_clear_mem(structMembers, sizeof(SsrSpirvStructMember) * numStructMembers);
    //
    // Step 2.3. Save struct and struct members ids
    //
    size_t structCounter = 0;
    size_t memberCounter = 0;
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        const uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        const uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpTypeStruct:
            {
                SsrSpirvStruct* shaderStruct = &structs[structCounter++];
                shaderStruct->id = &ids[instruction[1]];
                shaderStruct->numMembers = instructionWordCount - 2;
                shaderStruct->members = &structMembers[memberCounter];
                memberCounter += shaderStruct->numMembers;
                for (size_t it = 0; it < shaderStruct->numMembers; it++)
                {
                    SsrSpirvStructMember* member = &shaderStruct->members[it];
                    member->id = &ids[instruction[2 + it]];
                }
            } break;
        }
        instruction += instructionWordCount;
    }
    //
    // Step 2.4. Save struct member names
    //
    for (instruction = bytecode + 5; instruction < (bytecode + wordCount);)
    {
        const uint16_t instructionOpCode = ssr_opcode(instruction[0]);
        const uint16_t instructionWordCount = (uint16_t)(instruction[0] >> 16);
        switch (instructionOpCode)
        {
            case SpvOpMemberName:
            {
                ssr_assert(instructionWordCount >= 4);
                const SsrSpirvWord memberIndex = instruction[2];
                SsrSpirvId* structId = &ids[instruction[1]];
                SsrSpirvStruct* structure = NULL;
                for (size_t it = 0; it < numStructs; it++)
                {
                    if (structs[it].id == structId)
                    {
                        structure = &structs[it];
                        break;
                    }
                }
                ssr_assert(structure);
                structure->members[memberIndex].name = (const char*)&instruction[3];
            } break;
        }
        instruction += instructionWordCount;
    }
    //
    // Step 3. Process all input, ouptut variables, push constants and buffers
    //
    for (size_t it = 0; it < bound; it++)
    {
        SsrSpirvId* id = &ids[it];
        if (id->declarationLocation && ssr_opcode(id->declarationLocation[0]) == SpvOpVariable)
        {
            const SpvStorageClass storageClass = (SpvStorageClass)ids[id->declarationLocation[2]].storageClass;
            if (storageClass == SpvStorageClassInput)
            {
                SsrShaderIO* inputVariable = &reflection->inputs[reflection->numInputs++];
                __ssr_process_shader_io_variable(structs, numStructs, ids, id, reflection, inputVariable);
            }
            else if (storageClass == SpvStorageClassOutput)
            {
                SsrShaderIO* outputVariable = &reflection->outputs[reflection->numOutputs++];
                __ssr_process_shader_io_variable(structs, numStructs, ids, id, reflection, outputVariable);
            }
            else if (storageClass == SpvStorageClassPushConstant)
            {
                reflection->pushConstantName = __ssr_save_string(id->name, reflection);
                // Start from variable declaration
                ssr_assert(ssr_opcode(id->declarationLocation[0]) == SpvOpVariable);
                SsrSpirvId* localId = &ids[id->declarationLocation[1]];
                ssr_assert(ssr_opcode(localId->declarationLocation[0]) == SpvOpTypePointer);
                ssr_assert(localId->declarationLocation[2] == SpvStorageClassPushConstant);
                // Go to OpTypeStruct value referenced in OpTypePointer
                localId = &ids[localId->declarationLocation[3]];
                ssr_assert(ssr_opcode(localId->declarationLocation[0]) == SpvOpTypeStruct);
                reflection->pushConstantType = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, localId, reflection);
            }
            else if (storageClass == SpvStorageClassUniform)
            {
                __ssr_process_shader_uniform(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassUniformConstant)
            {
                __ssr_process_shader_uniform(structs, numStructs, ids, id, reflection);
            }
            else if (storageClass == SpvStorageClassStorageBuffer)
            {
                __ssr_process_shader_uniform(structs, numStructs, ids, id, reflection);
            }
        }
        else if (id->declarationLocation &&
                    (ssr_opcode(id->declarationLocation[0]) == SpvOpSpecConstant        ||
                     ssr_opcode(id->declarationLocation[0]) == SpvOpSpecConstantFalse   ||
                     ssr_opcode(id->declarationLocation[0]) == SpvOpSpecConstantTrue    ))
        {
            SsrSpecializationConstant* constant = &reflection->specializationConstants[reflection->numSpecializationConstants++];
            ssr_assert(id->decorations & SSR_DECORATION_SPEC_ID);
            SsrSpirvId* typeId = &ids[id->declarationLocation[1]];
            constant->name = __ssr_save_string(id->name, reflection);
            constant->constantId = id->specId;
            constant->type = __ssr_save_or_get_type_from_reflection(structs, numStructs, ids, typeId, reflection);
            switch (constant->type->type)
            {
                case SSR_TYPE_SCALAR:
                {
                    if (constant->type->info.scalar.isInteger)
                    {
                        ssr_assert(constant->type->info.scalar.bitWidth == 32 && "Only 32 bit integers are supported for specialization constants");
                        if (constant->type->info.scalar.isSigned)
                        {
                            constant->asUint = *((uint32_t*)&id->declarationLocation[3]);
                        }
                        else
                        {
                            constant->asInt = *((int32_t*)&id->declarationLocation[3]);
                        }
                    }
                    else
                    {
                        ssr_assert(constant->type->info.scalar.bitWidth == 32 && "Only 32 bit float is supported for specialization constants");
                        constant->asFloat = *((float*)&id->declarationLocation[3]);
                    }
                } break;
                case SSR_TYPE_BOOL:
                {
                    constant->asBool = ssr_opcode(id->declarationLocation[0]) == SpvOpSpecConstantTrue;
                } break;
                default: { ssr_assert("Unsupported specialization constant type"); }
            }
        }
    }
    //
    // Cleanup
    //
    nonPersistentAllocator->free(nonPersistentAllocator->userData, structs, sizeof(SsrSpirvStruct) * numStructs);
    nonPersistentAllocator->free(nonPersistentAllocator->userData, structMembers, sizeof(SsrSpirvStructMember) * numStructMembers);
    nonPersistentAllocator->free(nonPersistentAllocator->userData, ids, sizeof(SsrSpirvId) * bound);
#undef ssr_save_decl_location
}

void ssr_destroy(SimpleSpirvReflection* reflection)
{
    SsrMemoryBlock* block = reflection->memory;
    while (block)
    {
        SsrMemoryBlock* next = block->next;
        reflection->allocator.free(reflection->allocator.userData, block, block->capacity + sizeof(SsrMemoryBlock));
        block = next;
    }
}

size_t ssr_get_type_size(const SsrTypeInfo* typeInfo)
{
    switch(typeInfo->type)
    {
        case SSR_TYPE_BOOL:
        {
            return 1;
        } break;
        case SSR_TYPE_SCALAR:
        {
            return typeInfo->info.scalar.bitWidth / 8;
        } break;
        case SSR_TYPE_VECTOR:
        {
            return typeInfo->info.vector.numComponents * ssr_get_type_size(typeInfo->info.vector.componentType);
        } break;
        case SSR_TYPE_MATRIX:
        {
            return typeInfo->info.matrix.numColumns * ssr_get_type_size(typeInfo->info.matrix.columnType);
        } break;
        case SSR_TYPE_STRUCT:
        {
            size_t resultSize = 0;
            for (size_t it = 0; it < typeInfo->info.structure.numMembers; it++)
            {
                resultSize += ssr_get_type_size(typeInfo->info.structure.members[it]);
            }
            return resultSize;
        } break;
        case SSR_TYPE_SAMPLER:
        {
            return 0; // ?
        } break;
        case SSR_TYPE_IMAGE:
        {
            return 0; // ?
        } break;
        case SSR_TYPE_ARRAY:
        {
            return typeInfo->info.array.size * ssr_get_type_size(typeInfo->info.array.entryType);
        } break;
        default: { ssr_assert(!"Unknown SSR_TYPE_*"); }
    }
    return 0;
}

const char* ssr_shader_type_to_str(SsrShaderType shader)
{
    static const char* types[] =
    {
        "vertex",
        "fragment",
    };
    return types[shader];
}

const char* ssr_type_to_str(SsrType type)
{
    static const char* types[] =
    {
        "bool",
        "scalar",
        "vector",
        "matrix",
        "struct",
        "sampler",
        "image",
        "array",
    };
    return types[type];
}

const char* ssr_uniform_kind_to_str(SsrUniformKind kind)
{
    static const char* kinds[] =
    {
        "sampler",
        "sampled image",
        "storage image",
        "combined image sampler",
        "uniform texel buffer",
        "storage texel buffer",
        "uniform buffer",
        "storage buffer",
        "input attachment",
        "acceleration structure",
    };
    return kinds[kind];
}

#undef ssr_opcode

#ifdef __cplusplus
}
#endif

#endif //SSR_IMPL_INNER
#endif //SSR_IMPL
