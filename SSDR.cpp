#include "SSDR.h"
#include <limits>
#include <algorithm>
#include <Eigen/Core>
#include <Eigen/Eigen>
#include "QuadProg.h"

using namespace DirectX;
using namespace Eigen;

namespace SSDR {

double ComputeApproximationErrorSq(const Output& output, const Input& input, const Parameter& param)
{
    std::vector<double> errsq(input.numExamples);
    double rsqsum = 0;
    for (int s = 0; s < input.numExamples; ++s)
    {
        const int numVertices = input.numVertices;
        const int numIndices = param.numIndices;
        const int numBones = output.numBones;
        const int numExamples = input.numExamples;

        for (int s = 0; s < numExamples; ++s)
        {
            for (int v = 0; v < numVertices; ++v)
            {
                XMVECTOR residual = XMLoadFloat3A(&input.sample[s * numVertices + v]);
                const XMVECTOR& p = XMLoadFloat3A(&input.bindModel[v]);
                for (int i = 0; i < numIndices; ++i)
                {
                    const int b = output.index[v * numIndices + i];
                    const float w = output.weight[v * numIndices + i];
                    const RigidTransform& rt = output.boneTrans[s * numBones + b];
                    residual -= w * rt.TransformCoord(p);
                }
                rsqsum += XMVectorGetX(XMVector3LengthSq(residual));
            }
        }
    }
    return rsqsum;
}

void UpdateWeightMap(Output& output, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numExamples = input.numExamples;
    const int numIndices = param.numIndices;
    const int numBones = output.numBones;

    // 総和制約 : cem * xv + cev = 0
    MatrixXd cem = MatrixXd::Zero(1, numBones);
    MatrixXd scem = MatrixXd::Zero(1, numIndices);
    VectorXd cev = VectorXd::Zero(1);
    for (int b = 0; b < numBones; ++b)
    {
        cem(0, b) = 1.0;
    }
    cev(0) = -1.0;
    // 非負制約 : cim * xv + civ >= 0
    MatrixXd cim = MatrixXd::Zero(numBones, numBones);
    MatrixXd scim = MatrixXd::Zero(numIndices, numIndices);
    VectorXd civ = VectorXd::Zero(numBones);
    VectorXd sciv = VectorXd::Zero(numIndices);
    for (int b = 0; b < numBones; ++b)
    {
        cim(b, b) = 1.0;
        civ(b) = 0;
    }
    for (int i = 0; i < numIndices; ++i)
    {
        scem(0, i) = 1.0;
        scim(i, i) = 1.0;
        sciv(i) = 0;
    }
    
    MatrixXd gm = MatrixXd::Zero(numBones, numBones);
    MatrixXd sgm = MatrixXd::Zero(numIndices, numIndices);
    VectorXd gv = VectorXd::Zero(numBones);
    VectorXd sgv = VectorXd::Zero(numIndices);

    VectorXd weight = VectorXd::Zero(numBones);
    VectorXd sweight = VectorXd::Zero(numIndices);
    MatrixXd am = MatrixXd::Zero(numBones, numExamples * 3);
    MatrixXd sam = MatrixXd::Zero(numIndices, numExamples * 3);
    VectorXd bv = VectorXd::Zero(numExamples * 3);

    for (int v = 0; v < numVertices; ++v)
    {
        const XMVECTOR restVertex = XMLoadFloat3A(&input.bindModel[v]);
        for (int s = 0; s < numExamples; ++s)
        {
            for (int b = 0; b < numBones; ++b)
            {
                const RigidTransform& rt = output.boneTrans[s * numBones + b];
                XMVECTOR tv = rt.TransformCoord(restVertex);
                am(b, s * 3 + 0) = XMVectorGetX(tv);
                am(b, s * 3 + 1) = XMVectorGetY(tv);
                am(b, s * 3 + 2) = XMVectorGetZ(tv);
            }
        }
        for (int s = 0; s < numExamples; ++s)
        {
            bv[s * 3 + 0] = input.sample[s * numVertices + v].x;
            bv[s * 3 + 1] = input.sample[s * numVertices + v].y;
            bv[s * 3 + 2] = input.sample[s * numVertices + v].z;
        }
        // G = A * A^T
        gm = am * am.transpose();
        // g = A^T * b
        gv = -am * bv;

        double qperr = SolveQP(gm, gv, cem, cev, cim, civ, weight);
        assert(qperr != std::numeric_limits<double>::infinity());

        float weightSum = 0;
        for (int i = 0; i < numIndices; ++i)
        {
            double maxw = -std::numeric_limits<double>::max();
            int bestbone = -1;
            for (int b = 0; b < numBones; ++b)
            {
                if (weight[b] > maxw)
                {
                    maxw = weight[b];
                    bestbone = b;
                }
            }
            if (maxw <= 0)
            {
                break;
            }

            output.index[v * numIndices + i] = bestbone;
            output.weight[v * numIndices + i] = static_cast<float>(maxw);
            weightSum += static_cast<float>(maxw);
            weight[bestbone] = 0;
        }

        if (weightSum < 1.0f)
        {
            for (int j = 0; j < numExamples * 3; ++j)
            {
                for (int i = 0; i < numIndices; ++i)
                {
                    sam(i, j) = am(output.index[v * numIndices + i], j);
                }
            }
            sgm = sam * sam.transpose();
            sgv = -sam * bv;
            qperr = SolveQP(sgm, sgv, scem, cev, scim, sciv, sweight);
            if (qperr != std::numeric_limits<double>::infinity())
            {
                for (int i = 0; i < numIndices; ++i)
                {
                    output.weight[v * numIndices + i] = static_cast<float>(sweight[i]);
                }
            }
            else
            {
                for (int i = 0; i < numIndices; ++i)
                {
                    output.weight[v * numIndices + i] /= weightSum;
                }
            }
        }
    }
}

// リストxxx.13：Hornの点群位置合わせアルゴリズム
RigidTransform CalcPointsAlignment(size_t numPoints, std::vector<XMFLOAT3A>::const_iterator ps, std::vector<XMFLOAT3A>::const_iterator pd)
{
    RigidTransform transform;

    // それぞれの点群の重心座標の計算
    XMVECTOR cs = XMVectorZero(), cd = XMVectorZero();
    std::vector<XMFLOAT3A>::const_iterator sit = ps;
    std::vector<XMFLOAT3A>::const_iterator dit = pd;
    for (size_t i = 0; i < numPoints; ++i, ++sit, ++dit)
    {
        cs += XMLoadFloat3A(&(*sit));
        cd += XMLoadFloat3A(&(*dit));
    }
    cs /= numPoints;
    cd /= numPoints;

    // モーメント行列の計算
    Matrix<double, 4, 4> moment;
    double sxx = 0, sxy = 0, sxz = 0, syx = 0, syy = 0, syz = 0, szx = 0, szy = 0, szz = 0;
    sit = ps;
    dit = pd;
    for (size_t i = 0; i < numPoints; ++i, ++sit, ++dit)
    {
        sxx += (sit->x - XMVectorGetX(cs)) * (dit->x - XMVectorGetX(cd));
        sxy += (sit->x - XMVectorGetX(cs)) * (dit->y - XMVectorGetY(cd));
        sxz += (sit->x - XMVectorGetX(cs)) * (dit->z - XMVectorGetZ(cd));
        syx += (sit->y - XMVectorGetY(cs)) * (dit->x - XMVectorGetX(cd));
        syy += (sit->y - XMVectorGetY(cs)) * (dit->y - XMVectorGetY(cd));
        syz += (sit->y - XMVectorGetY(cs)) * (dit->z - XMVectorGetZ(cd));
        szx += (sit->z - XMVectorGetZ(cs)) * (dit->x - XMVectorGetX(cd));
        szy += (sit->z - XMVectorGetZ(cs)) * (dit->y - XMVectorGetY(cd));
        szz += (sit->z - XMVectorGetZ(cs)) * (dit->z - XMVectorGetZ(cd));
    }
    moment(0, 0) = sxx + syy + szz;
    moment(0, 1) = syz - szy;        moment(1, 0) = moment(0, 1);
    moment(0, 2) = szx - sxz;        moment(2, 0) = moment(0, 2);
    moment(0, 3) = sxy - syx;        moment(3, 0) = moment(0, 3);
    moment(1, 1) = sxx - syy - szz;
    moment(1, 2) = sxy + syx;        moment(2, 1) = moment(1, 2);
    moment(1, 3) = szx + sxz;        moment(3, 1) = moment(1, 3);
    moment(2, 2) = -sxx + syy - szz;
    moment(2, 3) = syz + szy;        moment(3, 2) = moment(2, 3);
    moment(3, 3) = -sxx - syy + szz;

    if (moment.norm() > 0)
    {
        // 符号付き最大固有値に対応する固有ベクトルを取得
        EigenSolver<Matrix<double, 4, 4>> es(moment);
        int maxi = 0;
        for (int i = 1; i < 4; ++i)
        {
            if (es.eigenvalues()(maxi).real() < es.eigenvalues()(i).real())
            {
                maxi = i;
            }
        }
        transform.Rotation() = XMFLOAT4A(
            static_cast<float>(es.eigenvectors()(1, maxi).real()),
            static_cast<float>(es.eigenvectors()(2, maxi).real()),
            static_cast<float>(es.eigenvectors()(3, maxi).real()),
            static_cast<float>(es.eigenvectors()(0, maxi).real()));
    }

    // 平行移動成分
    //
    XMVECTOR cs0 = transform.TransformCoord(cs);
    XMStoreFloat3A(&transform.Translation(), cd - cs0);
    return transform;
}

// 式xxx.9：\tilde{q}_{j,n}
void ComputeExamplePoints(std::vector<XMFLOAT3A>& example, int sid, int bone, const Output& output, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numIndices = param.numIndices;
    const int numBones = output.numBones;
    for (int v = 0; v < numVertices; ++v)
    {
        example[v] = input.sample[sid * numVertices + v];
        const XMVECTOR s = XMLoadFloat3A(&input.bindModel[v]);
        for (int i = 0; i < numIndices; ++i)
        {
            const int b = output.index[v * numIndices + i];
            if (b != bone)
            {
                const float w = output.weight[v * numIndices + i];
                const RigidTransform& at = output.boneTrans[sid * numBones + b];
                XMVECTOR r = XMLoadFloat3A(&example[v]);
                XMStoreFloat3A(&example[v], r - w * at.TransformCoord(s));
            }
        }
    }
}

void SubtractCentroid(std::vector<XMFLOAT3A>& model, std::vector<XMFLOAT3A>& example, XMFLOAT3A& corModel, XMFLOAT3A& corExample, const VectorXd& weight, const Output& output, const Input& input)
{
    const int numVertices = input.numVertices;

    // 式xxx.10：\bar{p}_n，\bar{q}_{j,n}
    double wsqsum = 0;
    XMVECTOR dmodel = XMVectorZero();
    XMVECTOR dexample = XMVectorZero();
    for (int v = 0; v < numVertices; ++v)
    {
        const double w = weight[v];
        dmodel += static_cast<float>(w * w) * XMLoadFloat3A(&input.bindModel[v]);
        dexample += static_cast<float>(w)* XMLoadFloat3A(&example[v]);
        wsqsum += w * w;
    }
    dmodel /= static_cast<float>(wsqsum);
    dexample /= static_cast<float>(wsqsum);
    XMStoreFloat3A(&corModel, dmodel);
    XMStoreFloat3A(&corExample, dexample);

    for (int v = 0; v < numVertices; ++v)
    {
        // 式xxx.11：w_{j,c} p_j
        XMVECTOR d = XMLoadFloat3A(&input.bindModel[v]) - XMLoadFloat3A(&corModel);
        XMStoreFloat3A(&model[v], static_cast<float>(weight[v]) * d);
        // 式xxx.11：q_{j,n}
        d = XMLoadFloat3A(&example[v]) - static_cast<float>(weight[v]) * XMLoadFloat3A(&corExample);
        XMStoreFloat3A(&example[v], d);
    }
}

void UpdateBoneTransform(Output& output, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numExamples = input.numExamples;
    const int numIndices = param.numIndices;
    const int numBones = output.numBones;

    VectorXd weight = VectorXd::Zero(numVertices);
    std::vector<XMFLOAT3A> model(input.numVertices), example(input.numVertices);
    for (int bone = 0; bone < numBones; ++bone)
    {
        for (int v = 0; v < numVertices; ++v)
        {
            weight[v] = 0;
            for (int i = 0; i < numIndices; ++i)
            {
                if (output.index[v * numIndices + i] == bone)
                {
                    weight[v] = output.weight[v * numIndices + i];
                    break;
                }
            }
        }

        for (int s = 0; s < numExamples; ++s)
        {
            // 式xxx.9：\tilde{q}_{j,n}
            ComputeExamplePoints(example, s, bone, output, input, param);
            // 式xxx.10，xxx.11
            XMFLOAT3A corModel(0, 0, 0), corExample(0, 0, 0);
            SubtractCentroid(model, example, corModel, corExample, weight, output, input);
            // 式xxx.12の解
            RigidTransform transform = CalcPointsAlignment(model.size(), model.begin(), example.begin());
            // 式xxx.13
            XMVECTOR d = XMLoadFloat3A(&corExample) - transform.TransformCoord(XMLoadFloat3A(&corModel));
            XMStoreFloat3A(&transform.Translation(), d + XMLoadFloat3A(&transform.Translation()));
            output.boneTrans[s * output.numBones + bone] = transform;
        }
    }
}

void UpdateBoneTransform(std::vector<RigidTransform>& boneTrans, int numBones, const Output& output, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numExamples = input.numExamples;
    const int numIndices = param.numIndices;

    std::vector<int> numBoneVertices(numBones, 0);
    for (int v = 0; v < numVertices; ++v)
    {
        ++numBoneVertices[output.index[v * numIndices + 0]];
    }
    std::vector<int> boneVertexId(numBones, 0);
    for (int i = 1; i < numBones; ++i)
    {
        boneVertexId[i] = boneVertexId[i - 1] + numBoneVertices[i - 1];
    }
    std::vector<XMFLOAT3A> skin(numVertices, XMFLOAT3A(0, 0, 0));
    std::vector<XMFLOAT3A> anim(numVertices * numExamples, XMFLOAT3A(0, 0, 0));
    for (int v = 0; v < numVertices; ++v)
    {
        const int bs = output.index[v * numIndices + 0];
        const int bd = boneVertexId[bs];
        skin[bd] = input.bindModel[v];
        for (int s = 0; s < numExamples; ++s)
        {
            anim[s * numVertices + bd] = input.sample[s * numVertices + v];
        }
        ++boneVertexId[bs];
    }
    boneVertexId[0] = 0;
    for (int i = 1; i < numBones; ++i)
    {
        boneVertexId[i] = boneVertexId[i - 1] + numBoneVertices[i - 1];
    }
    for (int b = 0; b < numBones; ++b)
    {
        for (int s = 0; s < numExamples; ++s)
        {
            boneTrans[s * numBones + b] = CalcPointsAlignment(numBoneVertices[b], skin.begin() + boneVertexId[b], anim.begin() + s * numVertices + boneVertexId[b]);
        }
    }
}

int BindVertexToBone(Output& output, std::vector<RigidTransform>& boneTrans, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numExamples = input.numExamples;
    const int numIndices = param.numIndices;
    int numBones = static_cast<int>(boneTrans.size() / numExamples);

    std::vector<int> numBoneVertices(numBones, 0);
    std::vector<float> vertexError(numVertices, 0);

    for (int v = 0; v < numVertices; ++v)
    {
        int bestBone = 0;
        float minErr = std::numeric_limits<float>::max();
        const XMVECTOR bindModelPos = XMLoadFloat3A(&input.bindModel[v]);
        for (int b = 0; b < numBones; ++b)
        {
            float errsq = 0;
            for (int s = 0; s < numExamples; ++s)
            {
                const RigidTransform& at = boneTrans[s * numBones + b];
                XMVECTOR diff = XMLoadFloat3A(&input.sample[s * numVertices + v])
                              - at.TransformCoord(XMLoadFloat3A(&input.bindModel[v]));
                errsq += XMVectorGetX(XMVector3LengthSq(diff));
            }
            if (errsq < minErr)
            {
                bestBone = b;
                minErr = errsq;
            }
        }
        ++numBoneVertices[bestBone];
        output.index[v * numIndices + 0] = bestBone;
        vertexError[v] = minErr;
    }

    // 空クラスタの除去
    std::vector<int>::iterator smallestBoneSize = std::min_element(numBoneVertices.begin(), numBoneVertices.end());
    while (*smallestBoneSize <= 0)
    {
        const int smallestBone = static_cast<int>(smallestBoneSize - numBoneVertices.begin());
        numBoneVertices.erase(numBoneVertices.begin() + smallestBone);
        for (int s = input.numExamples - 1; s >= 0; --s)
        {
            boneTrans.erase(boneTrans.begin() + s * numBones + smallestBone);
        }
        for (int v = 0; v < numVertices; ++v)
        {
            int b = output.index[v * numIndices + 0];
            if (b >= smallestBone)
            {
                --output.index[v * numIndices + 0];
            }
        }
        --numBones;
        smallestBoneSize = std::min_element(numBoneVertices.begin(), numBoneVertices.end());
    }
    return static_cast<int>(numBoneVertices.size());
}

int ClusterInitialBones(Output& output, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numExamples = input.numExamples;
    const int numIndices = param.numIndices;

    std::fill(output.index.begin(), output.index.end(), 0);
    std::fill(output.weight.begin(), output.weight.end(), 0.0f);
    for (int v = 0; v < numVertices; ++v)
    {
        output.weight[v * numIndices + 0] = 1.0f;
    }

    int numClusters = 1;
    std::vector<RigidTransform> boneTrans(numExamples);
    UpdateBoneTransform(boneTrans, numClusters, output, input, param);

    while (numClusters < param.numMinBones)
    {
        std::vector<XMFLOAT3A> clusterCenter(numClusters, XMFLOAT3A(0, 0, 0));
        std::vector<int> numBoneVertices(numClusters, 0);
        for (int v = 0; v < numVertices; ++v)
        {
            const int c = output.index[v * numIndices + 0];
            clusterCenter[c].x += input.bindModel[v].x;
            clusterCenter[c].y += input.bindModel[v].y;
            clusterCenter[c].z += input.bindModel[v].z;
            ++numBoneVertices[c];
        }
        for (int c = 0; c < numClusters; ++c)
        {
            clusterCenter[c].x /= static_cast<float>(numBoneVertices[c]);
            clusterCenter[c].y /= static_cast<float>(numBoneVertices[c]);
            clusterCenter[c].z /= static_cast<float>(numBoneVertices[c]);
        }

        std::vector<float> maxClusterError(numClusters, -std::numeric_limits<float>::max());
        std::vector<int> mostDistantVertex(numClusters, -1);
        for (int v = 0; v < numVertices; ++v)
        {
            const int c = output.index[v * numIndices + 0];
            float sumApproxErrorSq = 0;
            for (int s = 0; s < numExamples; ++s)
            {
                XMVECTOR diff = XMLoadFloat3A(&input.sample[s * numVertices + v])
                    - boneTrans[s * numClusters + c].TransformCoord(XMLoadFloat3A(&input.bindModel[v]));
                sumApproxErrorSq += XMVectorGetX(XMVector3LengthSq(diff));
            }
            XMVECTOR d = XMLoadFloat3A(&input.bindModel[v]) - XMLoadFloat3A(&clusterCenter[c]);
            float errSq = sumApproxErrorSq * XMVectorGetX(XMVector3LengthSq(d));
            if (errSq > maxClusterError[c])
            {
                maxClusterError[c] = errSq;
                mostDistantVertex[c] = v;
            }
        }
        int numPrevClusters = numClusters;
        for (int c = 0; c < numPrevClusters; ++c)
        {
            output.index[mostDistantVertex[c] * numIndices + 0] = numClusters++;
            --numBoneVertices[c];
            numBoneVertices.push_back(1);
        }
        boneTrans.resize(numExamples * numClusters);

        UpdateBoneTransform(boneTrans, numClusters, output, input, param);
        numClusters = BindVertexToBone(output, boneTrans, input, param);
    }
    return numClusters;
}

#pragma region Decompose
double Decompose(Output& output, const Input& input, const Parameter& param)
{
    const int numVertices = input.numVertices;
    const int numExamples = input.numExamples;
    const int numIndices = param.numIndices;

    output.index.assign(numVertices * numIndices, 0);
    output.weight.assign(numVertices * numIndices, 0.0f);

    FILE* fout = stderr;
    //fopen_s(&fout, "convergence.csv", "w");

    // クラスタ分割期待値最大化法を用いた初期バインディング
    output.numBones = ClusterInitialBones(output, input, param);
    // 初期ボーントランスフォーム
    output.boneTrans.assign(numExamples * output.numBones, RigidTransform::Identity());
    UpdateBoneTransform(output.boneTrans, output.numBones, output, input, param);
    fprintf(fout, "Initial  , %f\n", ComputeApproximationErrorSq(output, input, param));

    // BCDアルゴリズムによるスキニングウェイトとボーン姿勢の交互最適化
    for (int loop = 0; loop < param.numMaxIterations; ++loop)
    {
        UpdateWeightMap(output, input, param);
        fprintf(fout, "WeightMap, %f\n", ComputeApproximationErrorSq(output, input, param));
        UpdateBoneTransform(output, input, param);
        fprintf(fout, "Transform, %f\n", ComputeApproximationErrorSq(output, input, param));
    }
    if (fout != stderr)
    {
        fclose(fout);
    }
    return ComputeApproximationErrorSq(output, input, param);
}
#pragma endregion

} //namespace SSDR