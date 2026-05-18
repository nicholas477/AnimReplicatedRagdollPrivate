// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Widgets/Views/STableRow.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class USkeletalMesh;
class SComboButton;
template<typename ElementType> class STreeView;

struct FBoneTreeItem
{
    FString BoneName;
    int32 BoneIndex;
    TArray<TSharedPtr<FBoneTreeItem>> Children;
    TWeakPtr<FBoneTreeItem> Parent;

    FBoneTreeItem(const FString& InBoneName, int32 InBoneIndex)
        : BoneName(InBoneName), BoneIndex(InBoneIndex)
    {
    }
};

/**
 * Property customization for FReplicatedRagdollBoneFilter that provides a hierarchical tree
 * to select bones from the parent SkeletalMeshComponent
 */
class FReplicatedRagdollBoneFilterDetails : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();

    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
    TSharedPtr<IPropertyHandle> BonePropertyHandle;
    TSharedPtr<SComboButton> BoneComboButton;
    TArray<TSharedPtr<FBoneTreeItem>> BoneTree;
    TSharedPtr<STreeView<TSharedPtr<FBoneTreeItem>>> BoneTreeView;
    USkeletalMesh* CachedSkeletalMesh;

    /** Refresh the bone tree from the skeletal mesh */
    void RefreshBoneTree();

    /** Generate menu content for bone selection */
    TSharedRef<SWidget> GenerateBoneMenuContent();

    /** Handle bone selection from the tree */
    void OnBoneSelected(TSharedPtr<FBoneTreeItem> BoneItem, ESelectInfo::Type SelectInfo);

    /** Generate a row for the bone tree */
    TSharedRef<ITableRow> GenerateBoneRow(TSharedPtr<FBoneTreeItem> BoneItem, const TSharedRef<STableViewBase>& OwnerTable);

    /** Get children of a tree item */
    void OnGetChildren(TSharedPtr<FBoneTreeItem> BoneItem, TArray<TSharedPtr<FBoneTreeItem>>& OutChildren) const;

    /** Get the currently selected bone name for display */
    FText GetSelectedBoneName() const;

    void OnBoneNameCommitted(const FText& InText, ETextCommit::Type InCommitType);

    /** Try to get the skeletal mesh from the property handle's outer object */
    USkeletalMesh* GetSkeletalMeshFromContext();

    /** Build the bone hierarchy tree */
    void BuildBoneTree(const USkeleton* Skeleton);

    /** Recursively add bones to tree */
    TSharedPtr<FBoneTreeItem> AddBoneToTree(int32 BoneIndex, const USkeleton* Skeleton, TWeakPtr<FBoneTreeItem> ParentItem);
};
