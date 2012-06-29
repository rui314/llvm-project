//===-- SWIG Interface for SBType -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace lldb {

    %feature("docstring",
"Represents a member of a type in lldb.
") SBTypeMember;

class SBTypeMember
{
public:
    SBTypeMember ();

    SBTypeMember (const lldb::SBTypeMember& rhs);

    ~SBTypeMember();

    bool
    IsValid() const;

    const char *
    GetName ();

    lldb::SBType
    GetType ();

    uint64_t
    GetOffsetInBytes();
    
    uint64_t
    GetOffsetInBits();
    
    %pythoncode %{
        __swig_getmethods__["name"] = GetName
        if _newclass: name = property(GetName, None, doc='''A read only property that returns the name for this member as a string.''')
        
        __swig_getmethods__["type"] = GetType
        if _newclass: type = property(GetType, None, doc='''A read only property that returns an lldb object that represents the type (lldb.SBType) for this member.''')
        
        __swig_getmethods__["byte_offset"] = GetOffsetInBytes
        if _newclass: byte_offset = property(GetOffsetInBytes, None, doc='''A read only property that returns offset in bytes for this member as an integer.''')
        
        __swig_getmethods__["bit_offset"] = GetOffsetInBits
        if _newclass: bit_offset = property(GetOffsetInBits, None, doc='''A read only property that returns offset in bits for this member as an integer.''')
    %}    

protected:
    std::auto_ptr<lldb_private::TypeMemberImpl> m_opaque_ap;
};

%feature("docstring",
"Represents a data type in lldb.  The FindFirstType() method of SBTarget/SBModule
returns a SBType.

SBType supports the eq/ne operator. For example,

main.cpp:

class Task {
public:
    int id;
    Task *next;
    Task(int i, Task *n):
        id(i),
        next(n)
    {}
};

int main (int argc, char const *argv[])
{
    Task *task_head = new Task(-1, NULL);
    Task *task1 = new Task(1, NULL);
    Task *task2 = new Task(2, NULL);
    Task *task3 = new Task(3, NULL); // Orphaned.
    Task *task4 = new Task(4, NULL);
    Task *task5 = new Task(5, NULL);

    task_head->next = task1;
    task1->next = task2;
    task2->next = task4;
    task4->next = task5;

    int total = 0;
    Task *t = task_head;
    while (t != NULL) {
        if (t->id >= 0)
            ++total;
        t = t->next;
    }
    printf('We have a total number of %d tasks\\n', total);

    // This corresponds to an empty task list.
    Task *empty_task_head = new Task(-1, NULL);

    return 0; // Break at this line
}

find_type.py:

        # Get the type 'Task'.
        task_type = target.FindFirstType('Task')
        self.assertTrue(task_type)

        # Get the variable 'task_head'.
        frame0.FindVariable('task_head')
        task_head_type = task_head.GetType()
        self.assertTrue(task_head_type.IsPointerType())

        # task_head_type is 'Task *'.
        task_pointer_type = task_type.GetPointerType()
        self.assertTrue(task_head_type == task_pointer_type)

        # Get the child mmember 'id' from 'task_head'.
        id = task_head.GetChildMemberWithName('id')
        id_type = id.GetType()

        # SBType.GetBasicType() takes an enum 'BasicType' (lldb-enumerations.h).
        int_type = id_type.GetBasicType(lldb.eBasicTypeInt)
        # id_type and int_type should be the same type!
        self.assertTrue(id_type == int_type)

...
") SBType;
class SBType
{
public:
    SBType ();

    SBType (const lldb::SBType &rhs);

    ~SBType ();

    bool
    IsValid();

    size_t
    GetByteSize();

    bool
    IsPointerType();

    bool
    IsReferenceType();

    lldb::SBType
    GetPointerType();

    lldb::SBType
    GetPointeeType();

    lldb::SBType
    GetReferenceType();

    lldb::SBType
    GetDereferencedType();

    lldb::SBType
    GetUnqualifiedType();
    
    lldb::SBType
    GetBasicType (lldb::BasicType type);

    uint32_t
    GetNumberOfFields ();
    
    uint32_t
    GetNumberOfDirectBaseClasses ();
    
    uint32_t
    GetNumberOfVirtualBaseClasses ();
    
    lldb::SBTypeMember
    GetFieldAtIndex (uint32_t idx);
    
    lldb::SBTypeMember
    GetDirectBaseClassAtIndex (uint32_t idx);
    
    lldb::SBTypeMember
    GetVirtualBaseClassAtIndex (uint32_t idx);

    const char*
    GetName();
    
    lldb::TypeClass
    GetTypeClass ();
    
    uint32_t
    GetNumberOfTemplateArguments ();
    
    lldb::SBType
    GetTemplateArgumentType (uint32_t idx);
    
    lldb::TemplateArgumentKind
    GetTemplateArgumentKind (uint32_t idx);
    
    bool
    IsTypeComplete ();

    %pythoncode %{
        def template_arg_array(self):
            num_args = self.num_template_args
            if num_args:
                template_args = []
                for i in range(num_args):
                    template_args.append(self.GetTemplateArgumentType(i))
                return template_args
            return None
            
        __swig_getmethods__["name"] = GetName
        if _newclass: name = property(GetName, None, doc='''A read only property that returns the name for this type as a string.''')
        
        __swig_getmethods__["size"] = GetByteSize
        if _newclass: size = property(GetByteSize, None, doc='''A read only property that returns size in bytes for this type as an integer.''')
        
        __swig_getmethods__["is_pointer"] = IsPointerType
        if _newclass: is_pointer = property(IsPointerType, None, doc='''A read only property that returns a boolean value that indicates if this type is a pointer type.''')
        
        __swig_getmethods__["is_reference"] = IsReferenceType
        if _newclass: is_reference = property(IsReferenceType, None, doc='''A read only property that returns a boolean value that indicates if this type is a reference type.''')

        __swig_getmethods__["num_fields"] = GetNumberOfFields
        if _newclass: num_fields = property(GetNumberOfFields, None, doc='''A read only property that returns number of fields in this type as an integer.''')
        
        __swig_getmethods__["num_bases"] = GetNumberOfDirectBaseClasses
        if _newclass: num_bases = property(GetNumberOfDirectBaseClasses, None, doc='''A read only property that returns number of direct base classes in this type as an integer.''')
        
        __swig_getmethods__["num_vbases"] = GetNumberOfVirtualBaseClasses
        if _newclass: num_vbases = property(GetNumberOfVirtualBaseClasses, None, doc='''A read only property that returns number of virtual base classes in this type as an integer.''')
        
        __swig_getmethods__["num_template_args"] = GetNumberOfTemplateArguments
        if _newclass: num_template_args = property(GetNumberOfTemplateArguments, None, doc='''A read only property that returns number of template arguments in this type as an integer.''')

        __swig_getmethods__["template_args"] = template_arg_array
        if _newclass: template_args = property(template_arg_array, None, doc='''A read only property that returns a list() of lldb.SBType objects that represent all template arguments in this type.''')

        __swig_getmethods__["type"] = GetTypeClass
        if _newclass: type = property(GetTypeClass, None, doc='''A read only property that returns an lldb enumeration value (see enumerations that start with "lldb.eTypeClass") that represents a classification for this type.''')
        
        __swig_getmethods__["is_complete"] = IsTypeComplete
        if _newclass: is_complete = property(IsTypeComplete, None, doc='''A read only property that returns a boolean value that indicates if this type is a complete type (True) or a forward declaration (False).''')
        %}

};

%feature("docstring",
"Represents a list of SBTypes.  The FindTypes() method of SBTarget/SBModule
returns a SBTypeList.

SBTypeList supports SBType iteration. For example,

main.cpp:

class Task {
public:
    int id;
    Task *next;
    Task(int i, Task *n):
        id(i),
        next(n)
    {}
};

...

find_type.py:

        # Get the type 'Task'.
        type_list = target.FindTypes('Task')
        self.assertTrue(len(type_list) == 1)
        # To illustrate the SBType iteration.
        for type in type_list:
            # do something with type

...
") SBTypeList;
class SBTypeList
{
public:
    SBTypeList();

    bool
    IsValid();

    void
    Append (lldb::SBType type);

    lldb::SBType
    GetTypeAtIndex (uint32_t index);

    uint32_t
    GetSize();

    ~SBTypeList();
};

} // namespace lldb
