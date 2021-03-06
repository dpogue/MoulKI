#include "qtSDLTreeModel.h"
#include <SDL/plStateDataRecord.h>
#include <SDL/plStateVariable.h>
#include <Stream/hsRAMStream.h>
#include <QStringList>

const char* TypeNames[] = {
    "INT", "FLOAT", "BOOL", "STRING", "KEY", "STATEDESC", "CREATABLE",
    "DOUBLE", "TIME", "BYTE", "SHORT", "AGETIMEOFDAY",
    "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "VECTOR3", "POINT3", "RGB", "RGBA", "QUATERNION", "RGB8", "RGBA8",
    "UINT", "CHAR", "MATRIX44", "BUFFER", "AGETIMEELAPSED", "GAMETIMEELAPSED"
};

enum { kDataRow, kTypeRow };

qtSDLTreeModel::qtSDLTreeModel(QObject* parent, plVaultBlob blob, plSDLMgr* sdlmgr, plResManager* resmgr) :
    QAbstractItemModel(parent), resmgr(resmgr)
{
    hsRAMStream S(PlasmaVer::pvMoul);
    S.copyFrom(blob.getData(), blob.getSize());
    int version;
    ST::string name;
    sdl = new plStateDataRecord;
    sdl->ReadStreamHeader(&S, name, version, NULL);
    sdl->setDescriptor(sdlmgr->GetDescriptor(name, version));
    sdl->read(&S, resmgr);
}

qtSDLTreeModel::~qtSDLTreeModel() {
    delete sdl;
}

QModelIndex qtSDLTreeModel::ICreateIndex(int row, int column, const QModelIndex& parent, void *ptr, ItemType type) const {
    SDLModelIndex index;
    index.ptr.raw = ptr;
    index.type = type;
    index.parent = parent;
    index.row = row;
    index.column = column;
    if(indices.contains(index)) {
        return createIndex(row, column, indices.indexOf(index));
    }else{
        indices.append(index);
        return createIndex(row, column, indices.count() - 1);
    }
}

QModelIndex qtSDLTreeModel::parent(const QModelIndex &child) const {
    return indices[child.internalId()].parent;
}

QModelIndex qtSDLTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if(parent.isValid()) {
        SDLModelIndex myParent = indices[parent.internalId()];
        switch(myParent.type) {
        case kSDR:
            return ICreateIndex(row, column, parent, myParent.ptr.sdr->get(row), kVar);
        case kVar:
            if(myParent.ptr.sv->getDescriptor()->getType() == plVarDescriptor::kStateDescriptor)
                return ICreateIndex(row, column, parent, ((plSDStateVariable*)myParent.ptr.sv)->Record(row), kSDR);
            else
                return ICreateIndex(row, column, parent, myParent.ptr.sv, kVal);
        case kVal:
            return QModelIndex(); // this is not valid, values have no children
        default:
            return QModelIndex(); // satisfy the compiler
        }
    }else{
        return ICreateIndex(row, column, parent, sdl, kSDR);
    }
}

int qtSDLTreeModel::rowCount(const QModelIndex& parent) const {
    if(parent.isValid()) {
        SDLModelIndex myParent = indices[parent.internalId()];
        switch(myParent.type) {
        case kSDR:
            return myParent.ptr.sdr->getNumVars();
        case kVar:
            return myParent.ptr.sv->getCount();
        case kVal:
            return 0;
        default:
            return 0;
        }
    }else{
        return 1; // only one root
    }
}

int qtSDLTreeModel::columnCount(const QModelIndex&) const {
    return 2;
}

// gets a prc string for any pointer
// to an object with a prcWrite method
template <class T>
QString getPrc(T v) {
    // make a prc object and write to it
    hsRAMStream s(PlasmaVer::pvMoul);
    pfPrcHelper prc(&s);
    v->prcWrite(&prc);
    // extract the contents of the stream
    char* str = new char[s.size() + 1];
    str[s.size()] = 0;
    s.copyTo(str, s.size());
    QString out(str);
    delete[] str;
    // strip off the xml header
    QStringList lst = out.split('\n');
    out = QString();
    for(int i = 2; i < lst.count(); i++)
        out.append(lst[i]);
    return out;
}
#include <QIcon>
QVariant qtSDLTreeModel::data(const QModelIndex& index, int role) const {
    if(role == Qt::DecorationRole)
        return QIcon(":ResourceFilename");
    if(role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();
    if(index.isValid()) {
        SDLModelIndex myIndex = indices[index.internalId()];
        switch(index.column()) {
        case kDataRow:
            switch(myIndex.type) {
            case kSDR:
                return myIndex.ptr.sdr->getDescriptor()->getName().c_str();
            case kVar:
                return myIndex.ptr.sv->getDescriptor()->getName().c_str();
            case kVal:
                {
                    plSimpleStateVariable* var = (plSimpleStateVariable*)myIndex.ptr.sv;
                    switch(var->getDescriptor()->getType()) {
                    case plVarDescriptor::kBool:
                        return var->Bool(index.row()) ? "True" : "False";
                    case plVarDescriptor::kInt:
                        return QString::number(var->Int(index.row()));
                    case plVarDescriptor::kByte:
                        return QString::number(var->Byte(index.row()));
                    case plVarDescriptor::kFloat:
                        return QString::number(var->Float(index.row()));
                    case plVarDescriptor::kDouble:
                        return QString::number(var->Double(index.row()));
                    case plVarDescriptor::kString:
                        return var->String(index.row()).c_str();
                    case plVarDescriptor::kChar:
                        return QString(var->Char(index.row()));
                    case plVarDescriptor::kKey:
                        return getPrc<plUoid*>(&var->Uoid(index.row()));
                    case plVarDescriptor::kCreatable:
                        return getPrc<plCreatable*>(var->Creatable(index.row()));
                    case plVarDescriptor::kRGB8:
                    case plVarDescriptor::kRGBA8:
                        return getPrc<hsColor32*>(&var->Color32(index.row()));
                    case plVarDescriptor::kRGB:
                    case plVarDescriptor::kRGBA:
                        return getPrc<hsColorRGBA*>(&var->ColorRGBA(index.row()));
                    case plVarDescriptor::kPoint3:
                    case plVarDescriptor::kVector3:
                        return getPrc<hsVector3*>(&var->Vector(index.row()));
                    case plVarDescriptor::kQuaternion:
                        return getPrc<hsQuat*>(&var->Quat(index.row()));
                    default:
                        return ST::format("Unhandled SDL Var Type ({})", var->getDescriptor()->getType()).c_str();
                    }
                }
            }
        case kTypeRow:
            switch(myIndex.type) {
            case kSDR:
                return "STATEDESC";
            case kVar:
                if(myIndex.ptr.sv->getDescriptor()->isVariableLength())
                    return ST::format("{}[]", TypeNames[myIndex.ptr.sv->getDescriptor()->getType()]).c_str();
                else
                    return ST::format("{}[{}]", TypeNames[myIndex.ptr.sv->getDescriptor()->getType()], myIndex.ptr.sv->getCount()).c_str();
            case kVal:
                return TypeNames[myIndex.ptr.sv->getDescriptor()->getType()];
            }
        }
    }
    return QVariant("Requested invalid index");
}

QVariant qtSDLTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch(section) {
    case kDataRow:
        return QVariant("Data");
    case kTypeRow:
        return QVariant("Type");
    default:
        return QVariant();
    }
}

Qt::ItemFlags qtSDLTreeModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if(indices[index.internalId()].type == kVal && index.column() == 0)
        return flags |= Qt::ItemIsEditable;
    return flags;
}

bool qtSDLTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(role != Qt::EditRole || value.type() != QVariant::String || index.column() != 0)
        return false;
    QString* strVal = (QString*)value.data();
    SDLModelIndex myIndex = indices[index.internalId()];
    if(myIndex.type != kVal)
        return false;
    plSimpleStateVariable* var = (plSimpleStateVariable*)myIndex.ptr.sv;
    switch(var->getDescriptor()->getType()) {
    case plVarDescriptor::kBool:
        if(*strVal == "True")
            var->Bool(index.row()) = true;
        else if(*strVal == "False")
            var->Bool(index.row()) = false;
        else
            return false;
        break;
    case plVarDescriptor::kInt:
    {
        bool result;
        int value = strVal->toInt(&result);
        if(result)
            var->Int(index.row()) = value;
        else
            return false;
        break;
    }
    case plVarDescriptor::kByte:
    {
        bool result;
        int value = strVal->toInt(&result);
        if(result && value >= 0 && value < 256)
            var->Byte(index.row()) = (unsigned char)value;
        else
            return false;
        break;
    }
    case plVarDescriptor::kChar:
    {
        char* str = strVal->toLocal8Bit().data();
        if(strlen(str) == 1)
            var->Char(index.row()) = str[0];
        else
            return false;
        break;
     }
    case plVarDescriptor::kFloat:
    {
        bool result;
        float value = strVal->toFloat(&result);
        if(result)
            var->Float(index.row()) = value;
        else
            return false;
        break;
    }
    case plVarDescriptor::kDouble:
    {
        bool result;
        double value = strVal->toDouble(&result);
        if(result)
            var->Double(index.row()) = value;
        else
            return false;
        break;
    }
    case plVarDescriptor::kString:
        var->String(index.row()) = strVal->toUtf8().constData();
        break;
    case plVarDescriptor::kKey:
    case plVarDescriptor::kCreatable:
    case plVarDescriptor::kRGB8:
    case plVarDescriptor::kRGBA8:
    case plVarDescriptor::kRGB:
    case plVarDescriptor::kRGBA:
    case plVarDescriptor::kPoint3:
    case plVarDescriptor::kVector3:
    case plVarDescriptor::kQuaternion:
    {
        pfPrcParser p;
        hsRAMStream s(PlasmaVer::pvMoul);
        s.copyFrom(strVal->toLocal8Bit().data(), strVal->toLocal8Bit().size());
        try {
            p.read(&s);
        } catch (const hsException& e) {
            qWarning("Exception parsing prc: %s", e.what());
            return false;
        }
        try {
            switch(var->getDescriptor()->getType()) {
            case plVarDescriptor::kKey:
                var->Uoid(index.row()).prcParse(p.getRoot());
                break;
            case plVarDescriptor::kCreatable:
                var->Creatable(index.row())->prcParse(p.getRoot(), resmgr);
                break;
            case plVarDescriptor::kRGB8:
            case plVarDescriptor::kRGBA8:
                var->Color32(index.row()).prcParse(p.getRoot());
                break;
            case plVarDescriptor::kRGB:
            case plVarDescriptor::kRGBA:
                var->ColorRGBA(index.row()).prcParse(p.getRoot());
                break;
            case plVarDescriptor::kPoint3:
            case plVarDescriptor::kVector3:
                var->Vector(index.row()).prcParse(p.getRoot());
                break;
            case plVarDescriptor::kQuaternion:
                var->Quat(index.row()).prcParse(p.getRoot());
                break;
            default:
                // this should not be possible
                Q_ASSERT(false);
            }
        } catch (const pfPrcTagException& e) {
            qWarning("Exception setting prc value: %s", e.what());
            return false;
        }
        break;
    }
    default:
        return false;
    }
    emit sdlEdited(sdl);
    return true;
}
