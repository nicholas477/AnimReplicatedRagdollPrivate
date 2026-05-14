// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/FReplicatedRagdollBoneFilterDetails.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "ReplicatedRagdollComponent.h"
#include "Animation/Skeleton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "FReplicatedRagdollBoneFilterDetails"

TSharedRef<IPropertyTypeCustomization> FReplicatedRagdollBoneFilterDetails::MakeInstance()
{
    return MakeShareable(new FReplicatedRagdollBoneFilterDetails());
}

void FReplicatedRagdollBoneFilterDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    BonePropertyHandle = PropertyHandle;
    CachedSkeletalMesh = nullptr;
    BoneTree.Empty();

    // Try to get the skeletal mesh from context
    GetSkeletalMeshFromContext();

    // Refresh bone tree
    RefreshBoneTree();

    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            SAssignNew(BoneComboButton, SComboButton)
            .OnGetMenuContent(this, &FReplicatedRagdollBoneFilterDetails::GenerateBoneMenuContent)
            .ButtonContent()
            [
                SNew(STextBlock)
                .Text(this, &FReplicatedRagdollBoneFilterDetails::GetSelectedBoneName)
            ]
        ];
}

void FReplicatedRagdollBoneFilterDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    // No children to customize for this simple struct
}

USkeletalMesh* FReplicatedRagdollBoneFilterDetails::GetSkeletalMeshFromContext()
{
    if (!BonePropertyHandle.IsValid())
    {
        return nullptr;
    }

    // Get the outer objects being edited
    TArray<UObject*> ExternalObjects;
    BonePropertyHandle->GetOuterObjects(ExternalObjects);

    for (UObject* Obj : ExternalObjects)
    {
        if (!Obj) continue;

        // Check if it's a ReplicatedRagdollComponent
        if (UReplicatedRagdollComponent* RagdollComponent = Cast<UReplicatedRagdollComponent>(Obj))
        {
            if (USkeletalMeshComponent* SkeletalMesh = RagdollComponent->GetSkeletalMesh())
            {
                CachedSkeletalMesh = SkeletalMesh->GetSkeletalMeshAsset();
                return CachedSkeletalMesh;
            }
        }

        // Check if it's an Actor that might have a SkeletalMeshComponent
        if (AActor* Actor = Cast<AActor>(Obj))
        {
            USkeletalMeshComponent* SkeletalMesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
            if (SkeletalMesh)
            {
                CachedSkeletalMesh = SkeletalMesh->GetSkeletalMeshAsset();
                return CachedSkeletalMesh;
            }
        }
    }

    return nullptr;
}

void FReplicatedRagdollBoneFilterDetails::RefreshBoneTree()
{
    BoneTree.Empty();

    if (CachedSkeletalMesh == nullptr)
    {
        GetSkeletalMeshFromContext();
    }

    if (CachedSkeletalMesh == nullptr)
    {
        return;
    }

    // Get skeleton and build tree
    if (const USkeleton* Skeleton = CachedSkeletalMesh->GetSkeleton())
    {
        BuildBoneTree(Skeleton);
    }

    if (BoneTreeView.IsValid())
    {
        BoneTreeView->RebuildList();
    }
}

void FReplicatedRagdollBoneFilterDetails::BuildBoneTree(const USkeleton* Skeleton)
{
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

    // Start with root bones (bones with parent index -1)
    for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
    {
        const FMeshBoneInfo& BoneInfo = RefSkeleton.GetRefBoneInfo()[i];
        if (BoneInfo.ParentIndex == INDEX_NONE || BoneInfo.ParentIndex < 0)
        {
            TSharedPtr<FBoneTreeItem> RootItem = AddBoneToTree(i, Skeleton, nullptr);
            if (RootItem.IsValid())
            {
                BoneTree.Add(RootItem);
            }
        }
    }
}

TSharedPtr<FBoneTreeItem> FReplicatedRagdollBoneFilterDetails::AddBoneToTree(int32 BoneIndex, const USkeleton* Skeleton, TWeakPtr<FBoneTreeItem> ParentItem)
{
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const FMeshBoneInfo& BoneInfo = RefSkeleton.GetRefBoneInfo()[BoneIndex];

    TSharedPtr<FBoneTreeItem> BoneItem = MakeShareable(new FBoneTreeItem(BoneInfo.Name.ToString(), BoneIndex));
    BoneItem->Parent = ParentItem;

    // Add children
    for (int32 i = BoneIndex + 1; i < RefSkeleton.GetNum(); ++i)
    {
        const FMeshBoneInfo& ChildBoneInfo = RefSkeleton.GetRefBoneInfo()[i];
        if (ChildBoneInfo.ParentIndex == BoneIndex)
        {
            TSharedPtr<FBoneTreeItem> ChildItem = AddBoneToTree(i, Skeleton, BoneItem);
            if (ChildItem.IsValid())
            {
                BoneItem->Children.Add(ChildItem);
            }
        }
    }

    return BoneItem;
}

TSharedRef<SWidget> FReplicatedRagdollBoneFilterDetails::GenerateBoneMenuContent()
{
    RefreshBoneTree();

    TSharedPtr<STreeView<TSharedPtr<FBoneTreeItem>>> NewTreeView;
    TSharedRef<SWidget> TreeWidget = SAssignNew(BoneTreeView, STreeView<TSharedPtr<FBoneTreeItem>>)
        .TreeItemsSource(&BoneTree)
        .OnGenerateRow(this, &FReplicatedRagdollBoneFilterDetails::GenerateBoneRow)
        .OnGetChildren(this, &FReplicatedRagdollBoneFilterDetails::OnGetChildren)
        .OnSelectionChanged(this, &FReplicatedRagdollBoneFilterDetails::OnBoneSelected);

    // Expand all items
    TFunction<void(const TSharedPtr<FBoneTreeItem>&)> ExpandAllChildren;
    ExpandAllChildren = [this, &ExpandAllChildren](const TSharedPtr<FBoneTreeItem>& Item)
    {
        if (Item.IsValid() && BoneTreeView.IsValid())
        {
            BoneTreeView->SetItemExpansion(Item, true);
            for (const TSharedPtr<FBoneTreeItem>& Child : Item->Children)
            {
                ExpandAllChildren(Child);
            }
        }
    };

    for (const TSharedPtr<FBoneTreeItem>& RootItem : BoneTree)
    {
        ExpandAllChildren(RootItem);
    }

    return SNew(SBox)
        .MaxDesiredHeight(500.0f)
        [
            TreeWidget
        ];
}

void FReplicatedRagdollBoneFilterDetails::OnGetChildren(TSharedPtr<FBoneTreeItem> BoneItem, TArray<TSharedPtr<FBoneTreeItem>>& OutChildren) const
{
    if (BoneItem.IsValid())
    {
        OutChildren = BoneItem->Children;
    }
}

void FReplicatedRagdollBoneFilterDetails::OnBoneSelected(TSharedPtr<FBoneTreeItem> BoneItem, ESelectInfo::Type SelectInfo)
{
    if (BoneItem.IsValid() && BonePropertyHandle.IsValid())
    {
        // Get the child handle for the Bone property
        TSharedPtr<IPropertyHandle> BoneChildHandle = BonePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FReplicatedRagdollBoneFilter, Bone));
        if (BoneChildHandle.IsValid())
        {
            BoneChildHandle->SetValue(FName(*BoneItem->BoneName));
        }

        if (BoneComboButton.IsValid())
        {
            BoneComboButton->SetIsOpen(false);
        }
    }
}

TSharedRef<ITableRow> FReplicatedRagdollBoneFilterDetails::GenerateBoneRow(TSharedPtr<FBoneTreeItem> BoneItem, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FBoneTreeItem>>, OwnerTable)
        [
            SNew(STextBlock)
            .Text(FText::FromString(BoneItem->BoneName))
        ];
}

FText FReplicatedRagdollBoneFilterDetails::GetSelectedBoneName() const
{
    if (!BonePropertyHandle.IsValid())
    {
        return LOCTEXT("SelectBone", "Select Bone...");
    }

    // Get the child handle for the Bone property
    TSharedPtr<IPropertyHandle> BoneChildHandle = BonePropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FReplicatedRagdollBoneFilter, Bone));
    if (!BoneChildHandle.IsValid())
    {
        return LOCTEXT("SelectBone", "Select Bone...");
    }

    FName CurrentBone;
    if (BoneChildHandle->GetValue(CurrentBone) == FPropertyAccess::Success)
    {
        if (CurrentBone.IsNone())
        {
            return LOCTEXT("SelectBone", "Select Bone...");
        }
        return FText::FromName(CurrentBone);
    }

    return LOCTEXT("SelectBone", "Select Bone...");
}

#undef LOCTEXT_NAMESPACE
