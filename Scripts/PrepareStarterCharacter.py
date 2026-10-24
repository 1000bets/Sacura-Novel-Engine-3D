import argparse
import hashlib
import json
import math
import struct
import uuid
from pathlib import Path


def Identity(Name):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, 'sacura/starter-humanoid/' + Name))


def WriteAsset(PathValue, Data, AssetType, SubAssets=None):
    PathValue.parent.mkdir(parents=True, exist_ok=True)
    PathValue.write_bytes(Data)
    Metadata = {'schemaVersion':1,'guid':Identity(PathValue.name),'assetType':AssetType,
                'loadSettings':{},'importInfo':{},'subAssets':SubAssets or []}
    PathValue.with_name(PathValue.name+'.meta').write_text(json.dumps(Metadata,indent=2))


def Prepare(Source, Destination):
    SourceBytes = Source.read_bytes()
    JsonLength = struct.unpack_from('<I',SourceBytes,12)[0]
    Document = json.loads(SourceBytes[20:20+JsonLength])
    BinaryOffset = 20+JsonLength
    BinaryLength = struct.unpack_from('<I',SourceBytes,BinaryOffset)[0]
    Binary = bytearray(SourceBytes[BinaryOffset+8:BinaryOffset+8+BinaryLength])

    def ReadAccessor(Identifier):
        Accessor = Document['accessors'][Identifier]
        View = Document['bufferViews'][Accessor['bufferView']]
        Format = {5121:'B',5123:'H',5126:'f'}[Accessor['componentType']]
        Width = {'SCALAR':1,'VEC3':3,'VEC4':4,'MAT4':16}[Accessor['type']]
        Size = struct.calcsize('<'+Format*Width)
        Start = View.get('byteOffset',0)+Accessor.get('byteOffset',0)
        return [list(struct.unpack_from('<'+Format*Width,Binary,Start+Index*View.get('byteStride',Size))) for Index in range(Accessor['count'])]

    def AddAccessor(Values, Type, ComponentType=5126):
        Binary.extend(bytes((-len(Binary))%4))
        Start = len(Binary)
        Format = {5123:'H',5126:'f'}[ComponentType]
        for Value in Values:
            Binary.extend(struct.pack('<'+Format*len(Value),*Value))
        View = len(Document['bufferViews'])
        Document['bufferViews'].append({'buffer':0,'byteOffset':Start,'byteLength':len(Binary)-Start})
        Accessor = {'bufferView':View,'componentType':ComponentType,'count':len(Values),'type':Type}
        if Type == 'SCALAR':
            Accessor['min'] = [min(Value[0] for Value in Values)]
            Accessor['max'] = [max(Value[0] for Value in Values)]
        Document['accessors'].append(Accessor)
        return len(Document['accessors'])-1

    Canonical = Document['skins'][0]
    CanonicalMatrices = ReadAccessor(Canonical['inverseBindMatrices'])
    for Node in Document['nodes']:
        if 'skin' not in Node or Node['skin'] == 0:
            continue
        Skin = Document['skins'][Node['skin']]
        Matrices = ReadAccessor(Skin['inverseBindMatrices'])
        Remap = []
        for Joint, Matrix in zip(Skin['joints'],Matrices):
            if Joint not in Canonical['joints']:
                raise ValueError('Mesh skin contains a joint absent from the primary skin')
            Index = Canonical['joints'].index(Joint)
            if max(abs(First-Second) for First,Second in zip(Matrix,CanonicalMatrices[Index])) > 0.0001:
                raise ValueError('Mesh skins have incompatible inverse bind matrices')
            Remap.append(Index)
        for Primitive in Document['meshes'][Node['mesh']]['primitives']:
            Original = ReadAccessor(Primitive['attributes']['JOINTS_0'])
            Primitive['attributes']['JOINTS_0'] = AddAccessor([[Remap[Value] for Value in Vertex] for Vertex in Original],'VEC4',5123)
        Node['skin'] = 0
    Document['skins'] = [Canonical]
    Dance = Document['animations'][0]
    Dance['name'] = 'HipHopDance'
    Document['animations'] = [Dance]

    def Multiply(First, Second):
        return [First[3]*Second[0]+First[0]*Second[3]+First[1]*Second[2]-First[2]*Second[1],
                First[3]*Second[1]-First[0]*Second[2]+First[1]*Second[3]+First[2]*Second[0],
                First[3]*Second[2]+First[0]*Second[1]-First[1]*Second[0]+First[2]*Second[3],
                First[3]*Second[3]-First[0]*Second[0]-First[1]*Second[1]-First[2]*Second[2]]

    Parents = {Child:Index for Index,Node in enumerate(Document['nodes']) for Child in Node.get('children',[])}
    def RestRotation(Index):
        Rotation = Document['nodes'][Index].get('rotation',[0,0,0,1])
        if Index in Parents:
            return Multiply(RestRotation(Parents[Index]),Rotation)
        return Rotation

    for Name, Duration in [('Idle',3.0),('Walk',0.8)]:
        Times = [Duration*Frame/60 for Frame in range(61)]
        TimeAccessor = AddAccessor([[Time] for Time in Times],'SCALAR')
        Animation = {'name':Name,'samplers':[],'channels':[]}
        for Joint in Canonical['joints']:
            Node = Document['nodes'][Joint]
            Role = Node['name'].split(':')[-1]
            WorldRotation = RestRotation(Joint)
            InverseRotation = [-Value for Value in WorldRotation[:3]]+[WorldRotation[3]]
            Rotations = []
            for Time in Times:
                Cycle = math.sin(Time/Duration*math.tau)
                Pitch = 0.0
                Roll = 0.0
                if Role in ('LeftArm','RightArm'):
                    Roll = 1.15
                    if Role == 'LeftArm':
                        Roll = -1.15
                if Name == 'Walk':
                    if Role in ('LeftUpLeg','RightArm'):
                        Pitch = 0.48*Cycle
                    elif Role in ('RightUpLeg','LeftArm'):
                        Pitch = -0.48*Cycle
                    elif Role == 'LeftLeg':
                        Pitch = -0.65*max(0,-Cycle)
                    elif Role == 'RightLeg':
                        Pitch = -0.65*max(0,Cycle)
                elif Role == 'Spine2':
                    Pitch = 0.012*Cycle
                LocalRotation = Multiply([math.sin(Pitch/2),0,0,math.cos(Pitch/2)],[0,0,math.sin(Roll/2),math.cos(Roll/2)])
                LocalRotation = Multiply(Multiply(InverseRotation,LocalRotation),WorldRotation)
                Rotations.append(Multiply(Node.get('rotation',[0,0,0,1]),LocalRotation))
            Output = AddAccessor(Rotations,'VEC4')
            Animation['channels'].append({'sampler':len(Animation['samplers']),'target':{'node':Joint,'path':'rotation'}})
            Animation['samplers'].append({'input':TimeAccessor,'output':Output,'interpolation':'LINEAR'})
        Document['animations'].append(Animation)
    Document['buffers'] = [{'byteLength':len(Binary)}]
    JsonBytes = json.dumps(Document,separators=(',',':')).encode()
    JsonBytes += b' '*((-len(JsonBytes))%4)
    Binary.extend(bytes((-len(Binary))%4))
    Output = struct.pack('<III',0x46546C67,2,28+len(JsonBytes)+len(Binary))
    Output += struct.pack('<II',len(JsonBytes),0x4E4F534A)+JsonBytes
    Output += struct.pack('<II',len(Binary),0x004E4942)+Binary
    ModelPath = Destination/'StarterHumanoid.glb'
    SubAssets = []
    for Type,Name,Kind,Index in [('Skeleton','HumanoidSkeleton','skin',0),('SkeletalMesh','HumanoidMesh','skin',0)]:
        SubAssets.append({'id':Identity(Name),'type':Type,'name':Name,'selector':{'kind':Kind,'index':Index}})
    for Index, Animation in enumerate(Document['animations']):
        SubAssets.append({'id':Identity(Animation['name']),'type':'AnimationClip','name':Animation['name'],'selector':{'kind':'animation','index':Index}})
    WriteAsset(ModelPath,Output,'Model',SubAssets)
    def AssetKey(Name):
        return {'asset':Identity(ModelPath.name),'subAsset':Identity(Name)}
    Character = {'format':'sakura.character','version':1,'name':'Starter Humanoid','description':'Skinned humanoid with idle, walk and Hip Hop Dance clips.',
                 'enabled':True,'model':AssetKey('HumanoidMesh'),'skeleton':AssetKey('HumanoidSkeleton'),
                 'dialogueWidget':'','idleAnimation':'idle','walkAnimation':'walk','color':[1,1,1,1],'animations':[]}
    for Identifier,Name in [('idle','Idle'),('walk','Walk'),('dance','HipHopDance')]:
        Character['animations'].append({'id':Identifier,'name':Name,'description':'','clip':AssetKey(Name),'pose':Identifier,'speed':1,'enabled':True,'loop':True,'inPlace':True})
    WriteAsset(Destination/'StarterHumanoid.character',json.dumps(Character,indent=2).encode(),'Character')
    for Index,Material in enumerate(Document['materials']):
        Properties = Material['pbrMetallicRoughness']
        MaterialInstance = {'schemaVersion':1,'parent':{'assetId':'42b33bd1-5bf7-5462-920b-297de12b0f18','subAssetId':None},
                            'overrides':{'BaseColor':{'type':'Color','value':Properties['baseColorFactor']},
                                         'Metallic':{'type':'Float','value':Properties['metallicFactor']},
                                         'Roughness':{'type':'Float','value':Properties['roughnessFactor']}}}
        WriteAsset(Destination/('HumanoidMaterial'+str(Index)+'.materialinstance'),json.dumps(MaterialInstance,indent=2).encode(),'MaterialInstance')
    print(json.dumps({'character':Identity('StarterHumanoid.character'),'model':Identity(ModelPath.name),'sourceSha256':hashlib.sha256(SourceBytes).hexdigest(),'animations':[Entry['name'] for Entry in Document['animations']]}))


if __name__ == '__main__':
    Parser = argparse.ArgumentParser()
    Parser.add_argument('Source',type=Path)
    Parser.add_argument('Destination',type=Path)
    Arguments = Parser.parse_args()
    Prepare(Arguments.Source,Arguments.Destination)
