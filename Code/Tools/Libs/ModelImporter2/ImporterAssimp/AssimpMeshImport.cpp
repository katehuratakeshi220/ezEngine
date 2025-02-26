#include <ModelImporter2/ModelImporterPCH.h>

#include <Foundation/Logging/Log.h>
#include <ModelImporter2/ImporterAssimp/ImporterAssimp.h>

#include <Foundation/Math/Float16.h>
#include <Foundation/Utilities/GraphicsUtils.h>
#include <RendererCore/Meshes/MeshBufferUtils.h>
#include <RendererCore/Meshes/MeshResourceDescriptor.h>
#include <assimp/scene.h>
#include <mikktspace/mikktspace.h>

namespace ezModelImporter2
{
  struct StreamIndices
  {
    ezUInt32 uiPositions = ezInvalidIndex;
    ezUInt32 uiNormals = ezInvalidIndex;
    ezUInt32 uiUV0 = ezInvalidIndex;
    ezUInt32 uiUV1 = ezInvalidIndex;
    ezUInt32 uiTangents = ezInvalidIndex;
    ezUInt32 uiBoneIdx = ezInvalidIndex;
    ezUInt32 uiBoneWgt = ezInvalidIndex;
    ezUInt32 uiColor0 = ezInvalidIndex;
    ezUInt32 uiColor1 = ezInvalidIndex;
  };

  ezResult ImporterAssimp::ProcessAiMesh(aiMesh* pMesh, const ezMat4& transform)
  {
    if ((pMesh->mPrimitiveTypes & aiPrimitiveType::aiPrimitiveType_TRIANGLE) == 0) // no triangles in there ?
      return EZ_SUCCESS;

    {
      auto& mi = m_MeshInstances[pMesh->mMaterialIndex].ExpandAndGetRef();
      mi.m_GlobalTransform = transform;
      mi.m_pMesh = pMesh;

      m_uiTotalMeshVertices += pMesh->mNumVertices;
      m_uiTotalMeshTriangles += pMesh->mNumFaces;
    }

    return EZ_SUCCESS;
  }

  static void SetMeshTriangleIndices(ezMeshBufferResourceDescriptor& mb, const aiMesh* pMesh, ezUInt32 uiTriangleIndexOffset, ezUInt32 uiVertexIndexOffset, bool bFlipTriangles)
  {
    if (bFlipTriangles)
    {
      for (ezUInt32 triIdx = 0; triIdx < pMesh->mNumFaces; ++triIdx)
      {
        const ezUInt32 finalTriIdx = uiTriangleIndexOffset + triIdx;

        const ezUInt32 f0 = pMesh->mFaces[triIdx].mIndices[0];
        const ezUInt32 f1 = pMesh->mFaces[triIdx].mIndices[1];
        const ezUInt32 f2 = pMesh->mFaces[triIdx].mIndices[2];

        mb.SetTriangleIndices(finalTriIdx, uiVertexIndexOffset + f0, uiVertexIndexOffset + f2, uiVertexIndexOffset + f1);
      }
    }
    else
    {
      for (ezUInt32 triIdx = 0; triIdx < pMesh->mNumFaces; ++triIdx)
      {
        const ezUInt32 finalTriIdx = uiTriangleIndexOffset + triIdx;

        const ezUInt32 f0 = pMesh->mFaces[triIdx].mIndices[0];
        const ezUInt32 f1 = pMesh->mFaces[triIdx].mIndices[1];
        const ezUInt32 f2 = pMesh->mFaces[triIdx].mIndices[2];

        mb.SetTriangleIndices(finalTriIdx, uiVertexIndexOffset + f0, uiVertexIndexOffset + f1, uiVertexIndexOffset + f2);
      }
    }
  }

  static void SetMeshBoneData(ezMeshBufferResourceDescriptor& mb, ezMeshResourceDescriptor& mrd, float& inout_fMaxBoneOffset, const aiMesh* pMesh, ezUInt32 uiVertexIndexOffset, const StreamIndices& streams, bool b8BitBoneIndices)
  {
    if (!pMesh->HasBones())
      return;

    ezHashedString hs;

    for (ezUInt32 b = 0; b < pMesh->mNumBones; ++b)
    {
      const aiBone* pBone = pMesh->mBones[b];

      hs.Assign(pBone->mName.C_Str());
      const ezUInt32 uiBoneIndex = mrd.m_Bones[hs].m_uiBoneIndex;

      for (ezUInt32 w = 0; w < pBone->mNumWeights; ++w)
      {
        const auto& weight = pBone->mWeights[w];

        const ezUInt32 finalVertIdx = uiVertexIndexOffset + weight.mVertexId;

        ezUInt8* pBoneIndices8 = reinterpret_cast<ezUInt8*>(mb.GetVertexData(streams.uiBoneIdx, finalVertIdx).GetPtr());
        ezUInt16* pBoneIndices16 = reinterpret_cast<ezUInt16*>(mb.GetVertexData(streams.uiBoneIdx, finalVertIdx).GetPtr());
        ezUInt8* pBoneWeights = reinterpret_cast<ezUInt8*>(mb.GetVertexData(streams.uiBoneWgt, finalVertIdx).GetPtr());

        ezUInt32 uiLeastWeightIdx = 0;

        for (int i = 1; i < 4; ++i)
        {
          if (pBoneWeights[i] < pBoneWeights[uiLeastWeightIdx])
          {
            uiLeastWeightIdx = i;
          }
        }

        const ezUInt8 uiEncodedWeight = ezMath::ColorFloatToByte(weight.mWeight);

        if (pBoneWeights[uiLeastWeightIdx] < uiEncodedWeight)
        {
          pBoneWeights[uiLeastWeightIdx] = uiEncodedWeight;

          if (b8BitBoneIndices)
            pBoneIndices8[uiLeastWeightIdx] = static_cast<ezUInt8>(uiBoneIndex);
          else
            pBoneIndices16[uiLeastWeightIdx] = static_cast<ezUInt16>(uiBoneIndex);
        }
      }
    }

    // normalize the bone weights
    // NOTE: This is absolutely crucial for some meshes to work right
    // On the other hand, it is also possible that some meshes don't like this
    // if we come across meshes where normalization breaks them, we may need to add a user-option to select whether bone weights should be normalized
    for (ezUInt32 vtx = 0; vtx < mb.GetVertexCount(); ++vtx)
    {
      const ezVec3 vVertexPos = *reinterpret_cast<const ezVec3*>(mb.GetVertexData(streams.uiPositions, vtx).GetPtr());
      const ezUInt8* pBoneIndices8 = reinterpret_cast<const ezUInt8*>(mb.GetVertexData(streams.uiBoneIdx, vtx).GetPtr());
      const ezUInt16* pBoneIndices16 = reinterpret_cast<const ezUInt16*>(mb.GetVertexData(streams.uiBoneIdx, vtx).GetPtr());
      ezUInt8* pBoneWeights = reinterpret_cast<ezUInt8*>(mb.GetVertexData(streams.uiBoneWgt, vtx).GetPtr());

      const ezUInt32 len = pBoneWeights[0] + pBoneWeights[1] + pBoneWeights[2] + pBoneWeights[3];

      ezVec4 wgt;
      wgt.x = ezMath::ColorByteToFloat(pBoneWeights[0]);
      wgt.y = ezMath::ColorByteToFloat(pBoneWeights[1]);
      wgt.z = ezMath::ColorByteToFloat(pBoneWeights[2]);
      wgt.w = ezMath::ColorByteToFloat(pBoneWeights[3]);

      if (len > 255)
      {
        wgt /= (len / 255.0f);

        pBoneWeights[0] = ezMath::ColorFloatToByte(wgt.x);
        pBoneWeights[1] = ezMath::ColorFloatToByte(wgt.y);
        pBoneWeights[2] = ezMath::ColorFloatToByte(wgt.z);
        pBoneWeights[3] = ezMath::ColorFloatToByte(wgt.w);
      }

      // also find the maximum distance of any vertex to its influencing bones
      // this is used to adjust the bounding box for culling at runtime
      // ie. we can compute the bounding box from a pose, but that only covers the skeleton, not the full mesh
      // so we then grow the bbox by this maximum distance
      // that usually creates a far larger bbox than necessary, but means there are no culling artifacts

      ezUInt16 uiBoneId[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

      if (b8BitBoneIndices)
      {
        uiBoneId[0] = pBoneIndices8[0];
        uiBoneId[1] = pBoneIndices8[1];
        uiBoneId[2] = pBoneIndices8[2];
        uiBoneId[3] = pBoneIndices8[3];
      }
      else
      {
        uiBoneId[0] = pBoneIndices16[0];
        uiBoneId[1] = pBoneIndices16[1];
        uiBoneId[2] = pBoneIndices16[2];
        uiBoneId[3] = pBoneIndices16[3];
      }

      for (const auto& bone : mrd.m_Bones)
      {
        for (int b = 0; b < 4; ++b)
        {
          if (wgt.GetData()[b] < 0.2f) // only look at bones that have a proper weight
            continue;

          if (bone.Value().m_uiBoneIndex == uiBoneId[b])
          {
            // move the vertex into local space of the bone, then determine how far it is away from the bone
            const ezVec3 vOffPos = bone.Value().m_GlobalInverseBindPoseMatrix * vVertexPos;
            const float len = vOffPos.GetLength();

            if (len > inout_fMaxBoneOffset)
            {
              inout_fMaxBoneOffset = len;
            }
          }
        }
      }
    }
  }

  static void SetMeshVertexData(ezMeshBufferResourceDescriptor& mb, const aiMesh* pMesh, const ezMat4& globalTransform, ezUInt32 uiVertexIndexOffset, const StreamIndices& streams, ezEnum<ezMeshNormalPrecision> meshNormalsPrecision, ezEnum<ezMeshTexCoordPrecision> meshTexCoordsPrecision)
  {
    ezMat3 normalsTransform = globalTransform.GetRotationalPart();
    if (normalsTransform.Invert(0.0f).Failed())
    {
      ezLog::Warning("Couldn't invert a mesh's transform matrix.");
      normalsTransform.SetIdentity();
    }

    normalsTransform.Transpose();

    for (ezUInt32 vertIdx = 0; vertIdx < pMesh->mNumVertices; ++vertIdx)
    {
      const ezUInt32 finalVertIdx = uiVertexIndexOffset + vertIdx;

      const ezVec3 position = globalTransform * ConvertAssimpType(pMesh->mVertices[vertIdx]);
      mb.SetVertexData(streams.uiPositions, finalVertIdx, position);

      if (streams.uiNormals != ezInvalidIndex && pMesh->HasNormals())
      {
        ezVec3 normal = normalsTransform * ConvertAssimpType(pMesh->mNormals[vertIdx]);
        normal.NormalizeIfNotZero(ezVec3::ZeroVector()).IgnoreResult();

        ezMeshBufferUtils::EncodeNormal(normal, mb.GetVertexData(streams.uiNormals, finalVertIdx), meshNormalsPrecision).IgnoreResult();
      }

      if (streams.uiUV0 != ezInvalidIndex && pMesh->HasTextureCoords(0))
      {
        const ezVec2 texcoord = ConvertAssimpType(pMesh->mTextureCoords[0][vertIdx]).GetAsVec2();

        ezMeshBufferUtils::EncodeTexCoord(texcoord, mb.GetVertexData(streams.uiUV0, finalVertIdx), meshTexCoordsPrecision).IgnoreResult();
      }

      if (streams.uiUV1 != ezInvalidIndex && pMesh->HasTextureCoords(1))
      {
        const ezVec2 texcoord = ConvertAssimpType(pMesh->mTextureCoords[1][vertIdx]).GetAsVec2();

        ezMeshBufferUtils::EncodeTexCoord(texcoord, mb.GetVertexData(streams.uiUV1, finalVertIdx), meshTexCoordsPrecision).IgnoreResult();
      }

      if (streams.uiColor0 != ezInvalidIndex && pMesh->HasVertexColors(0))
      {
        const ezColorLinearUB color = ConvertAssimpType(pMesh->mColors[0][vertIdx]);
        mb.SetVertexData(streams.uiColor0, finalVertIdx, color);
      }

      if (streams.uiColor1 != ezInvalidIndex && pMesh->HasVertexColors(1))
      {
        const ezColorLinearUB color = ConvertAssimpType(pMesh->mColors[1][vertIdx]);
        mb.SetVertexData(streams.uiColor1, finalVertIdx, color);
      }

      if (streams.uiTangents != ezInvalidIndex && pMesh->HasTangentsAndBitangents())
      {
        ezVec3 normal = normalsTransform * ConvertAssimpType(pMesh->mNormals[vertIdx]);
        ezVec3 tangent = normalsTransform * ConvertAssimpType(pMesh->mTangents[vertIdx]);
        ezVec3 bitangent = normalsTransform * ConvertAssimpType(pMesh->mBitangents[vertIdx]);

        normal.NormalizeIfNotZero(ezVec3::ZeroVector()).IgnoreResult();
        tangent.NormalizeIfNotZero(ezVec3::ZeroVector()).IgnoreResult();
        bitangent.NormalizeIfNotZero(ezVec3::ZeroVector()).IgnoreResult();

        const float fBitangentSign = ezMath::Abs(tangent.CrossRH(bitangent).Dot(normal));

        ezMeshBufferUtils::EncodeTangent(tangent, fBitangentSign, mb.GetVertexData(streams.uiTangents, finalVertIdx), meshNormalsPrecision).IgnoreResult();
      }
    }
  }

  static void AllocateMeshStreams(ezMeshBufferResourceDescriptor& mb, ezArrayPtr<aiMesh*> referenceMeshes, StreamIndices& streams, ezUInt32 uiTotalMeshVertices, ezUInt32 uiTotalMeshTriangles, ezEnum<ezMeshNormalPrecision> meshNormalsPrecision, ezEnum<ezMeshTexCoordPrecision> meshTexCoordsPrecision,
    bool bImportSkinningData, bool b8BitBoneIndices)
  {
    streams.uiPositions = mb.AddStream(ezGALVertexAttributeSemantic::Position, ezGALResourceFormat::XYZFloat);
    streams.uiNormals = mb.AddStream(ezGALVertexAttributeSemantic::Normal, ezMeshNormalPrecision::ToResourceFormatNormal(meshNormalsPrecision));
    streams.uiUV0 = mb.AddStream(ezGALVertexAttributeSemantic::TexCoord0, ezMeshTexCoordPrecision::ToResourceFormat(meshTexCoordsPrecision));
    streams.uiTangents = mb.AddStream(ezGALVertexAttributeSemantic::Tangent, ezMeshNormalPrecision::ToResourceFormatTangent(meshNormalsPrecision));

    if (bImportSkinningData)
    {
      if (b8BitBoneIndices)
        streams.uiBoneIdx = mb.AddStream(ezGALVertexAttributeSemantic::BoneIndices0, ezGALResourceFormat::RGBAUByte);
      else
        streams.uiBoneIdx = mb.AddStream(ezGALVertexAttributeSemantic::BoneIndices0, ezGALResourceFormat::RGBAUShort);

      streams.uiBoneWgt = mb.AddStream(ezGALVertexAttributeSemantic::BoneWeights0, ezGALResourceFormat::RGBAUByteNormalized);
    }

    bool bTexCoords1 = false;
    bool bVertexColors0 = false;
    bool bVertexColors1 = false;

    for (auto pMesh : referenceMeshes)
    {
      if (pMesh->HasTextureCoords(1))
        bTexCoords1 = true;
      if (pMesh->HasVertexColors(0))
        bVertexColors0 = true;
      if (pMesh->HasVertexColors(1))
        bVertexColors1 = true;
    }

    if (bTexCoords1)
    {
      streams.uiUV1 = mb.AddStream(ezGALVertexAttributeSemantic::TexCoord1, ezMeshTexCoordPrecision::ToResourceFormat(meshTexCoordsPrecision));
    }

    if (bVertexColors0)
    {
      streams.uiColor0 = mb.AddStream(ezGALVertexAttributeSemantic::Color0, ezGALResourceFormat::RGBAUByteNormalized);
    }
    if (bVertexColors1)
    {
      streams.uiColor1 = mb.AddStream(ezGALVertexAttributeSemantic::Color1, ezGALResourceFormat::RGBAUByteNormalized);
    }

    mb.AllocateStreams(uiTotalMeshVertices, ezGALPrimitiveTopology::Triangles, uiTotalMeshTriangles, true);
  }

  static void SetMeshBindPoseData(ezMeshResourceDescriptor& mrd, const aiMesh* pMesh, const ezMat4& globalTransform)
  {
    if (!pMesh->HasBones())
      return;

    ezHashedString hs;

    for (ezUInt32 b = 0; b < pMesh->mNumBones; ++b)
    {
      auto pBone = pMesh->mBones[b];

      auto invPose = ConvertAssimpType(pBone->mOffsetMatrix);
      EZ_VERIFY(invPose.Invert(0.0f).Succeeded(), "Inverting the bind pose matrix failed");
      invPose = globalTransform * invPose;
      EZ_VERIFY(invPose.Invert(0.0f).Succeeded(), "Inverting the bind pose matrix failed");

      hs.Assign(pBone->mName.C_Str());
      mrd.m_Bones[hs].m_GlobalInverseBindPoseMatrix = invPose;
    }
  }

  struct MikkData
  {
    ezMeshBufferResourceDescriptor* m_pMeshBuffer = nullptr;
    ezUInt32 m_uiVertexSize = 0;
    const ezUInt16* m_pIndices16 = nullptr;
    const ezUInt32* m_pIndices32 = nullptr;
    const ezUInt8* m_pPositions = nullptr;
    const ezUInt8* m_pNormals = nullptr;
    const ezUInt8* m_pTexCoords = nullptr;
    ezUInt8* m_pTangents = nullptr;
    ezGALResourceFormat::Enum m_NormalsFormat;
    ezGALResourceFormat::Enum m_TexCoordsFormat;
    ezGALResourceFormat::Enum m_TangentsFormat;
  };

  static int MikkGetNumFaces(const SMikkTSpaceContext* pContext)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    return pMikkData->m_pMeshBuffer->GetPrimitiveCount();
  }

  static int MikkGetNumVerticesOfFace(const SMikkTSpaceContext* pContext, int iFace)
  { //
    return 3;
  }

  static void MikkGetPosition16(const SMikkTSpaceContext* pContext, float outData[], int iFace, int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices16[iFace * 3 + iVert];

    const ezVec3* pData = reinterpret_cast<const ezVec3*>(pMikkData->m_pPositions + (uiVertexIdx * pMikkData->m_uiVertexSize));
    outData[0] = pData->x;
    outData[1] = pData->y;
    outData[2] = pData->z;
  }

  static void MikkGetPosition32(const SMikkTSpaceContext* pContext, float outData[], int iFace, int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);

    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices32[iFace * 3 + iVert];

    const ezVec3* pData = reinterpret_cast<const ezVec3*>(pMikkData->m_pPositions + (uiVertexIdx * pMikkData->m_uiVertexSize));
    outData[0] = pData->x;
    outData[1] = pData->y;
    outData[2] = pData->z;
  }

  static void MikkGetNormal16(const SMikkTSpaceContext* pContext, float outData[], int iFace, int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices16[iFace * 3 + iVert];

    ezVec3* pDest = reinterpret_cast<ezVec3*>(outData);
    ezMeshBufferUtils::DecodeNormal(ezConstByteArrayPtr(pMikkData->m_pNormals + (uiVertexIdx * pMikkData->m_uiVertexSize), 32), pMikkData->m_NormalsFormat, *pDest).IgnoreResult();
  }

  static void MikkGetNormal32(const SMikkTSpaceContext* pContext, float outData[], int iFace, int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices32[iFace * 3 + iVert];

    ezVec3* pDest = reinterpret_cast<ezVec3*>(outData);
    ezMeshBufferUtils::DecodeNormal(ezConstByteArrayPtr(pMikkData->m_pNormals + (uiVertexIdx * pMikkData->m_uiVertexSize), 32), pMikkData->m_NormalsFormat, *pDest).IgnoreResult();
  }

  static void MikkGetTexCoord16(const SMikkTSpaceContext* pContext, float outData[], int iFace, int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices16[iFace * 3 + iVert];

    ezVec2* pDest = reinterpret_cast<ezVec2*>(outData);
    ezMeshBufferUtils::DecodeTexCoord(ezConstByteArrayPtr(pMikkData->m_pTexCoords + (uiVertexIdx * pMikkData->m_uiVertexSize), 32), pMikkData->m_TexCoordsFormat, *pDest).IgnoreResult();
  }

  static void MikkGetTexCoord32(const SMikkTSpaceContext* pContext, float outData[], int iFace, int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices32[iFace * 3 + iVert];

    ezVec2* pDest = reinterpret_cast<ezVec2*>(outData);
    ezMeshBufferUtils::DecodeTexCoord(ezConstByteArrayPtr(pMikkData->m_pTexCoords + (uiVertexIdx * pMikkData->m_uiVertexSize), 32), pMikkData->m_TexCoordsFormat, *pDest).IgnoreResult();
  }

  static void MikkSetTangents16(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices16[iFace * 3 + iVert];

    const ezVec3 tangent = *reinterpret_cast<const ezVec3*>(fvTangent);

    ezMeshBufferUtils::EncodeTangent(tangent, fSign, ezByteArrayPtr(pMikkData->m_pTangents + (uiVertexIdx * pMikkData->m_uiVertexSize), 32), pMikkData->m_TangentsFormat).IgnoreResult();
  }

  static void MikkSetTangents32(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
  {
    MikkData* pMikkData = static_cast<MikkData*>(pContext->m_pUserData);
    const ezUInt32 uiVertexIdx = pMikkData->m_pIndices32[iFace * 3 + iVert];

    const ezVec3 tangent = *reinterpret_cast<const ezVec3*>(fvTangent);

    ezMeshBufferUtils::EncodeTangent(tangent, fSign, ezByteArrayPtr(pMikkData->m_pTangents + (uiVertexIdx * pMikkData->m_uiVertexSize), 32), pMikkData->m_TangentsFormat).IgnoreResult();
  }

  ezResult ImporterAssimp::RecomputeTangents()
  {
    auto& md = m_Options.m_pMeshOutput->MeshBufferDesc();

    MikkData mikkd;
    mikkd.m_pMeshBuffer = &md;
    mikkd.m_uiVertexSize = md.GetVertexDataSize();
    mikkd.m_pIndices16 = reinterpret_cast<const ezUInt16*>(md.GetIndexBufferData().GetData());
    mikkd.m_pIndices32 = reinterpret_cast<const ezUInt32*>(md.GetIndexBufferData().GetData());

    for (ezUInt32 i = 0; i < md.GetVertexDeclaration().m_VertexStreams.GetCount(); ++i)
    {
      const auto& stream = md.GetVertexDeclaration().m_VertexStreams[i];

      if (stream.m_Semantic == ezGALVertexAttributeSemantic::Position)
      {
        mikkd.m_pPositions = md.GetVertexData(i, 0).GetPtr();
      }
      else if (stream.m_Semantic == ezGALVertexAttributeSemantic::Normal)
      {
        mikkd.m_pNormals = md.GetVertexData(i, 0).GetPtr();
        mikkd.m_NormalsFormat = stream.m_Format;
      }
      else if (stream.m_Semantic == ezGALVertexAttributeSemantic::TexCoord0)
      {
        mikkd.m_pTexCoords = md.GetVertexData(i, 0).GetPtr();
        mikkd.m_TexCoordsFormat = stream.m_Format;
      }
      else if (stream.m_Semantic == ezGALVertexAttributeSemantic::Tangent)
      {
        mikkd.m_pTangents = md.GetVertexData(i, 0).GetPtr();
        mikkd.m_TangentsFormat = stream.m_Format;
      }
    }

    if (mikkd.m_pPositions == nullptr || mikkd.m_pTexCoords == nullptr || mikkd.m_pNormals == nullptr || mikkd.m_pTangents == nullptr)
      return EZ_FAILURE;

    // Use Morton S. Mikkelsen's tangent calculation.
    SMikkTSpaceContext context;
    SMikkTSpaceInterface functions;
    context.m_pUserData = &mikkd;
    context.m_pInterface = &functions;

    functions.m_setTSpace = nullptr;
    functions.m_getNumFaces = MikkGetNumFaces;
    functions.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;

    if (md.Uses32BitIndices())
    {
      functions.m_getPosition = MikkGetPosition32;
      functions.m_getNormal = MikkGetNormal32;
      functions.m_getTexCoord = MikkGetTexCoord32;
      functions.m_setTSpaceBasic = MikkSetTangents32;
    }
    else
    {
      functions.m_getPosition = MikkGetPosition16;
      functions.m_getNormal = MikkGetNormal16;
      functions.m_getTexCoord = MikkGetTexCoord16;
      functions.m_setTSpaceBasic = MikkSetTangents16;
    }

    if (!genTangSpaceDefault(&context))
      return EZ_FAILURE;

    return EZ_SUCCESS;
  }

  ezResult ImporterAssimp::PrepareOutputMesh()
  {
    if (m_Options.m_pMeshOutput == nullptr)
      return EZ_SUCCESS;

    auto& mb = m_Options.m_pMeshOutput->MeshBufferDesc();

    if (m_Options.m_bImportSkinningData)
    {
      for (auto itMesh : m_MeshInstances)
      {
        for (const auto& mi : itMesh.Value())
        {
          SetMeshBindPoseData(*m_Options.m_pMeshOutput, mi.m_pMesh, mi.m_GlobalTransform);
        }
      }

      ezUInt16 uiBoneCounter = 0;
      for (auto itBone : m_Options.m_pMeshOutput->m_Bones)
      {
        itBone.Value().m_uiBoneIndex = uiBoneCounter;
        ++uiBoneCounter;
      }
    }

    const bool b8BitBoneIndices = m_Options.m_pMeshOutput->m_Bones.GetCount() <= 255;

    StreamIndices streams;
    AllocateMeshStreams(mb, ezArrayPtr<aiMesh*>(m_aiScene->mMeshes, m_aiScene->mNumMeshes), streams, m_uiTotalMeshVertices, m_uiTotalMeshTriangles, m_Options.m_MeshNormalsPrecision, m_Options.m_MeshTexCoordsPrecision, m_Options.m_bImportSkinningData, b8BitBoneIndices);

    ezUInt32 uiMeshPrevTriangleIdx = 0;
    ezUInt32 uiMeshCurVertexIdx = 0;
    ezUInt32 uiMeshCurTriangleIdx = 0;
    ezUInt32 uiMeshCurSubmeshIdx = 0;

    const bool bFlipTriangles = ezGraphicsUtils::IsTriangleFlipRequired(m_Options.m_RootTransform);

    for (auto itMesh : m_MeshInstances)
    {
      const ezUInt32 uiMaterialIdx = itMesh.Key();

      for (const auto& mi : itMesh.Value())
      {
        SetMeshVertexData(mb, mi.m_pMesh, mi.m_GlobalTransform, uiMeshCurVertexIdx, streams, m_Options.m_MeshNormalsPrecision, m_Options.m_MeshTexCoordsPrecision);

        if (m_Options.m_bImportSkinningData)
        {
          SetMeshBoneData(mb, *m_Options.m_pMeshOutput, m_Options.m_pMeshOutput->m_fMaxBoneVertexOffset, mi.m_pMesh, uiMeshCurVertexIdx, streams, b8BitBoneIndices);
        }

        SetMeshTriangleIndices(mb, mi.m_pMesh, uiMeshCurTriangleIdx, uiMeshCurVertexIdx, bFlipTriangles);

        uiMeshCurTriangleIdx += mi.m_pMesh->mNumFaces;
        uiMeshCurVertexIdx += mi.m_pMesh->mNumVertices;
      }

      if (uiMaterialIdx >= m_OutputMaterials.GetCount())
      {
        m_Options.m_pMeshOutput->SetMaterial(uiMeshCurSubmeshIdx, "");
      }
      else
      {
        m_OutputMaterials[uiMaterialIdx].m_iReferencedByMesh = static_cast<ezInt32>(uiMeshCurSubmeshIdx);
        m_Options.m_pMeshOutput->SetMaterial(uiMeshCurSubmeshIdx, m_OutputMaterials[uiMaterialIdx].m_sName);
      }

      m_Options.m_pMeshOutput->AddSubMesh(uiMeshCurTriangleIdx - uiMeshPrevTriangleIdx, uiMeshPrevTriangleIdx, uiMeshCurSubmeshIdx);

      uiMeshPrevTriangleIdx = uiMeshCurTriangleIdx;
      ++uiMeshCurSubmeshIdx;
    }

    m_Options.m_pMeshOutput->ComputeBounds();

    return EZ_SUCCESS;
  }
} // namespace ezModelImporter2
