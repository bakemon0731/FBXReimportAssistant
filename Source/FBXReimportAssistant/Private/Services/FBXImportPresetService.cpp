#include "Services/FBXImportPresetService.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"


// ========================================
// 匿名名前空間
// ========================================

namespace
{
    // ------------------------------------
    // プリセット保存先を取得
    // ------------------------------------

    FString GetPresetDirectory()
    {
        return FPaths::Combine(
            FPaths::ProjectConfigDir(),
            TEXT("FBXReimportAssistant"),
            TEXT("Presets")
        );
    }


    // ------------------------------------
    // プリセット名の共通検証
    // ------------------------------------

    bool PreparePresetName(
        const FString& InputName,
        FString& OutCleanName,
        FString& OutSafeFileName,
        FString& OutMessage
    )
    {
        OutCleanName = InputName;
        OutSafeFileName.Empty();
        OutMessage.Empty();

        // 前後の空白を削除
        OutCleanName.TrimStartAndEndInline();


        // 空文字チェック
        if (OutCleanName.IsEmpty())
        {
            OutMessage = TEXT("プリセット名を入力してください。");
            return false;
        }


        // 文字数チェック
        if (OutCleanName.Len() > 64)
        {
            OutMessage = TEXT("プリセット名は64文字以内にしてください。");
            return false;
        }


        // ファイル名として安全な文字列を生成
        OutSafeFileName = FPaths::MakeValidFileName(
            OutCleanName,
            TCHAR('_')
        );


        // 不正なファイル名をチェック
        if (OutSafeFileName.IsEmpty() ||
            OutSafeFileName == TEXT(".") ||
            OutSafeFileName == TEXT(".."))
        {
            OutMessage = TEXT("使用できないプリセット名です。");
            return false;
        }


        // 不正文字が含まれていた場合は拒否
        if (OutSafeFileName != OutCleanName)
        {
            OutMessage = TEXT("ファイル名に使用できない文字が含まれています。");
            return false;
        }

        return true;
    }


    // ------------------------------------
    // プリセット数値の共通検証
    // ------------------------------------

    bool ValidatePresetSettings(
        const FFBXBatchOverrideSettings& Settings,
        FString& OutMessage
    )
    {
        OutMessage.Empty();


        // Translationの数値を検証
        if (!FMath::IsFinite(Settings.Translation.X) ||
            !FMath::IsFinite(Settings.Translation.Y) ||
            !FMath::IsFinite(Settings.Translation.Z))
        {
            OutMessage = TEXT("Translationに不正な数値があります。");
            return false;
        }


        // Rotationの数値を検証
        if (!FMath::IsFinite(Settings.Rotation.Roll) ||
            !FMath::IsFinite(Settings.Rotation.Pitch) ||
            !FMath::IsFinite(Settings.Rotation.Yaw))
        {
            OutMessage = TEXT("Rotationに不正な数値があります。");
            return false;
        }


        // Uniform Scaleの数値を検証
        if (!FMath::IsFinite(Settings.UniformScale))
        {
            OutMessage = TEXT("Uniform Scaleが不正な数値です。");
            return false;
        }


        // 上書きが有効な場合、Scaleの0を拒否
        if (Settings.bOverrideUniformScale &&
            Settings.UniformScale == 0.0f)
        {
            OutMessage = TEXT("Uniform Scaleに0は指定できません。");
            return false;
        }

        return true;
    }
}


// ========================================
// SavePreset
// プリセットをJSONファイルに保存
// ========================================

bool FBXImportPresetService::SavePreset(
    const FFBXImportPreset& Preset,
    bool bOverwriteExisting,
    FString& OutFilePath,
    FString& OutMessage
)
{
    OutFilePath.Empty();
    OutMessage.Empty();


    // ------------------------------------
    // 1. プリセット名を検証
    // ------------------------------------

    FString PresetName;
    FString SafeFileName;

    if (!PreparePresetName(
        Preset.Name,
        PresetName,
        SafeFileName,
        OutMessage
    ))
    {
        return false;
    }


    // ------------------------------------
    // 2. バージョンを検証
    // ------------------------------------

    if (Preset.Version != 1)
    {
        OutMessage = TEXT("未対応のプリセットバージョンです。");
        return false;
    }


    // ------------------------------------
    // 3. 数値を検証
    // ------------------------------------

    if (!ValidatePresetSettings(
        Preset.Settings,
        OutMessage
    ))
    {
        return false;
    }


    // ------------------------------------
    // 4. JSONルートオブジェクトを作成
    // ------------------------------------

    TSharedRef<FJsonObject> RootObject =
        MakeShared<FJsonObject>();


    RootObject->SetStringField(
        TEXT("Name"),
        PresetName
    );

    RootObject->SetNumberField(
        TEXT("Version"),
        Preset.Version
    );


    // ------------------------------------
    // 5. Settingsオブジェクトを作成
    // ------------------------------------

    TSharedRef<FJsonObject> SettingsObject =
        MakeShared<FJsonObject>();

    const FFBXBatchOverrideSettings& Settings =
        Preset.Settings;


    // チェック状態
    SettingsObject->SetBoolField(
        TEXT("OverrideTranslation"),
        Settings.bOverrideTranslation
    );

    SettingsObject->SetBoolField(
        TEXT("OverrideRotation"),
        Settings.bOverrideRotation
    );

    SettingsObject->SetBoolField(
        TEXT("OverrideUniformScale"),
        Settings.bOverrideUniformScale
    );


    // ------------------------------------
    // 6. Translationを保存
    // ------------------------------------

    TSharedRef<FJsonObject> TranslationObject =
        MakeShared<FJsonObject>();

    TranslationObject->SetNumberField(
        TEXT("X"),
        Settings.Translation.X
    );

    TranslationObject->SetNumberField(
        TEXT("Y"),
        Settings.Translation.Y
    );

    TranslationObject->SetNumberField(
        TEXT("Z"),
        Settings.Translation.Z
    );

    SettingsObject->SetObjectField(
        TEXT("Translation"),
        TranslationObject
    );


    // ------------------------------------
    // 7. Rotationを保存
    // ------------------------------------

    TSharedRef<FJsonObject> RotationObject =
        MakeShared<FJsonObject>();

    RotationObject->SetNumberField(
        TEXT("Pitch"),
        Settings.Rotation.Pitch
    );

    RotationObject->SetNumberField(
        TEXT("Yaw"),
        Settings.Rotation.Yaw
    );

    RotationObject->SetNumberField(
        TEXT("Roll"),
        Settings.Rotation.Roll
    );

    SettingsObject->SetObjectField(
        TEXT("Rotation"),
        RotationObject
    );


    // ------------------------------------
    // 8. Uniform Scaleを保存
    // ------------------------------------

    SettingsObject->SetNumberField(
        TEXT("UniformScale"),
        Settings.UniformScale
    );


    // RootObjectにSettingsを登録
    RootObject->SetObjectField(
        TEXT("Settings"),
        SettingsObject
    );


    // ------------------------------------
    // 9. JSON文字列に変換
    // ------------------------------------

    FString JsonString;

    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&JsonString);

    if (!FJsonSerializer::Serialize(
        RootObject,
        Writer
    ))
    {
        OutMessage = TEXT("JSONへの変換に失敗しました。");
        return false;
    }


    // ------------------------------------
    // 10. 保存先フォルダを取得
    // ------------------------------------

    const FString PresetDirectory =
        GetPresetDirectory();


    // フォルダが存在しない場合は作成
    IFileManager::Get().MakeDirectory(
        *PresetDirectory,
        true
    );


    // フォルダ作成を確認
    if (!FPaths::DirectoryExists(PresetDirectory))
    {
        OutMessage = TEXT("保存先フォルダを作成できませんでした。");
        return false;
    }


    // ------------------------------------
    // 11. 保存先ファイルパスを作成
    // ------------------------------------

    const FString FilePath = FPaths::Combine(
        PresetDirectory,
        SafeFileName + TEXT(".json")
    );


    // ------------------------------------
    // 12. 上書きの確認
    // ------------------------------------

    if (FPaths::FileExists(FilePath) &&
        !bOverwriteExisting)
    {
        OutMessage = TEXT("同名のプリセットが既に存在します。");
        return false;
    }


    // ------------------------------------
    // 13. JSONファイルを書き出す
    // ------------------------------------

    if (!FFileHelper::SaveStringToFile(
        JsonString,
        *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
    ))
    {
        OutMessage = TEXT("プリセットの保存に失敗しました。");
        return false;
    }


    OutFilePath = FilePath;

    OutMessage = TEXT("プリセットを保存しました。");

    return true;
}


// ========================================
// LoadPreset
// JSONファイルからプリセットを復元
// ========================================

bool FBXImportPresetService::LoadPreset(
    const FString& FilePath,
    FFBXImportPreset& OutPreset,
    FString& OutMessage
)
{
    OutPreset = FFBXImportPreset();
    OutMessage.Empty();


    // ------------------------------------
    // 1. ファイルパスを検証
    // ------------------------------------

    if (FilePath.IsEmpty())
    {
        OutMessage = TEXT("ファイルパスが指定されていません。");
        return false;
    }

    if (!FPaths::FileExists(FilePath))
    {
        OutMessage = TEXT("プリセットファイルが見つかりません。");
        return false;
    }


    // ------------------------------------
    // 2. JSONファイルを読み込む
    // ------------------------------------

    FString JsonString;

    if (!FFileHelper::LoadFileToString(
        JsonString,
        *FilePath
    ))
    {
        OutMessage = TEXT("プリセットファイルの読み込みに失敗しました。");
        return false;
    }


    // ------------------------------------
    // 3. JSON文字列を解析
    // ------------------------------------

    TSharedPtr<FJsonObject> RootObject;

    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(
        Reader,
        RootObject
    ) || !RootObject.IsValid())
    {
        OutMessage = TEXT("JSONの解析に失敗しました。");
        return false;
    }


    // ------------------------------------
    // 4. プリセット名を取得
    // ------------------------------------

    FFBXImportPreset LoadedPreset;

    if (!RootObject->TryGetStringField(
        TEXT("Name"),
        LoadedPreset.Name
    ))
    {
        OutMessage = TEXT("プリセット名が存在しません。");
        return false;
    }


    // 名前を共通検証
    FString CleanName;
    FString SafeFileName;

    if (!PreparePresetName(
        LoadedPreset.Name,
        CleanName,
        SafeFileName,
        OutMessage
    ))
    {
        return false;
    }

    LoadedPreset.Name = CleanName;


    // ------------------------------------
    // 5. バージョンを取得・検証
    // ------------------------------------

    double VersionValue = 0.0;

    if (!RootObject->TryGetNumberField(
        TEXT("Version"),
        VersionValue
    ))
    {
        OutMessage = TEXT("バージョン情報が存在しません。");
        return false;
    }

    if (!FMath::IsFinite(VersionValue) ||
        VersionValue != 1.0)
    {
        OutMessage = TEXT("未対応のプリセットバージョンです。");
        return false;
    }

    LoadedPreset.Version = 1;


    // ------------------------------------
    // 6. Settingsオブジェクトを取得
    // ------------------------------------

    const TSharedPtr<FJsonObject>* SettingsObjectPtr = nullptr;

    if (!RootObject->TryGetObjectField(
        TEXT("Settings"),
        SettingsObjectPtr
    ) ||
        SettingsObjectPtr == nullptr ||
        !SettingsObjectPtr->IsValid())
    {
        OutMessage = TEXT("Settingsが存在しません。");
        return false;
    }

    const TSharedPtr<FJsonObject>& SettingsObject =
        *SettingsObjectPtr;


    // ------------------------------------
    // 7. チェック状態を取得
    // ------------------------------------

    bool bOverrideTranslation = false;
    bool bOverrideRotation = false;
    bool bOverrideUniformScale = false;

    if (!SettingsObject->TryGetBoolField(
        TEXT("OverrideTranslation"),
        bOverrideTranslation
    ) ||
        !SettingsObject->TryGetBoolField(
            TEXT("OverrideRotation"),
            bOverrideRotation
        ) ||
        !SettingsObject->TryGetBoolField(
            TEXT("OverrideUniformScale"),
            bOverrideUniformScale
        ))
    {
        OutMessage = TEXT("設定のチェック状態を読み込めません。");
        return false;
    }


    // ------------------------------------
    // 8. Translationオブジェクトを取得
    // ------------------------------------

    const TSharedPtr<FJsonObject>* TranslationObjectPtr = nullptr;

    if (!SettingsObject->TryGetObjectField(
        TEXT("Translation"),
        TranslationObjectPtr
    ) ||
        TranslationObjectPtr == nullptr ||
        !TranslationObjectPtr->IsValid())
    {
        OutMessage = TEXT("Translationのデータがありません。");
        return false;
    }

    const TSharedPtr<FJsonObject>& TranslationObject =
        *TranslationObjectPtr;


    // Translationの数値を取得
    double TranslationX = 0.0;
    double TranslationY = 0.0;
    double TranslationZ = 0.0;

    if (!TranslationObject->TryGetNumberField(
        TEXT("X"),
        TranslationX
    ) ||
        !TranslationObject->TryGetNumberField(
            TEXT("Y"),
            TranslationY
        ) ||
        !TranslationObject->TryGetNumberField(
            TEXT("Z"),
            TranslationZ
        ))
    {
        OutMessage = TEXT("Translationの数値を読み込めません。");
        return false;
    }


    // ------------------------------------
    // 9. Rotationオブジェクトを取得
    // ------------------------------------

    const TSharedPtr<FJsonObject>* RotationObjectPtr = nullptr;

    if (!SettingsObject->TryGetObjectField(
        TEXT("Rotation"),
        RotationObjectPtr
    ) ||
        RotationObjectPtr == nullptr ||
        !RotationObjectPtr->IsValid())
    {
        OutMessage = TEXT("Rotationのデータがありません。");
        return false;
    }

    const TSharedPtr<FJsonObject>& RotationObject =
        *RotationObjectPtr;


    // Rotationの数値を取得
    double RotationPitch = 0.0;
    double RotationYaw = 0.0;
    double RotationRoll = 0.0;

    if (!RotationObject->TryGetNumberField(
        TEXT("Pitch"),
        RotationPitch
    ) ||
        !RotationObject->TryGetNumberField(
            TEXT("Yaw"),
            RotationYaw
        ) ||
        !RotationObject->TryGetNumberField(
            TEXT("Roll"),
            RotationRoll
        ))
    {
        OutMessage = TEXT("Rotationの数値を読み込めません。");
        return false;
    }


    // ------------------------------------
    // 10. Uniform Scaleを取得
    // ------------------------------------

    double UniformScaleValue = 1.0;

    if (!SettingsObject->TryGetNumberField(
        TEXT("UniformScale"),
        UniformScaleValue
    ))
    {
        OutMessage = TEXT("Uniform Scaleを読み込めません。");
        return false;
    }


    // ------------------------------------
    // 11. 構造体に値を復元
    // ------------------------------------

    LoadedPreset.Settings.bOverrideTranslation =
        bOverrideTranslation;

    LoadedPreset.Settings.bOverrideRotation =
        bOverrideRotation;

    LoadedPreset.Settings.bOverrideUniformScale =
        bOverrideUniformScale;


    LoadedPreset.Settings.Translation = FVector(
        TranslationX,
        TranslationY,
        TranslationZ
    );

    LoadedPreset.Settings.Rotation = FRotator(
        RotationPitch,
        RotationYaw,
        RotationRoll
    );

    LoadedPreset.Settings.UniformScale =
        static_cast<float>(UniformScaleValue);


    // ------------------------------------
    // 12. 共通数値検証
    // ------------------------------------

    if (!ValidatePresetSettings(
        LoadedPreset.Settings,
        OutMessage
    ))
    {
        return false;
    }


    // ------------------------------------
    // 13. 読み込み結果を返す
    // ------------------------------------

    OutPreset = MoveTemp(LoadedPreset);

    OutMessage = TEXT("プリセットを読み込みました。");

    return true;
}


// ========================================
// GetPresetFiles
// 保存済みプリセット一覧を取得
// ========================================

bool FBXImportPresetService::GetPresetFiles(
    TArray<FString>& OutFilePaths,
    FString& OutMessage
)
{
    OutFilePaths.Reset();
    OutMessage.Empty();


    // ------------------------------------
    // 1. 共通保存先を取得
    // ------------------------------------

    const FString PresetDirectory =
        GetPresetDirectory();


    // ------------------------------------
    // 2. フォルダの存在を確認
    // ------------------------------------

    if (!FPaths::DirectoryExists(PresetDirectory))
    {
        OutMessage = TEXT("プリセット保存フォルダが存在しません。");
        return true;
    }


    // ------------------------------------
    // 3. JSONファイルを検索
    // ------------------------------------

    TArray<FString> FoundFileNames;

    IFileManager::Get().FindFiles(
        FoundFileNames,
        *PresetDirectory,
        TEXT("json")
    );


    // ------------------------------------
    // 4. フルパスに変換
    // ------------------------------------

    for (const FString& FileName : FoundFileNames)
    {
        const FString FullPath = FPaths::Combine(
            PresetDirectory,
            FileName
        );

        OutFilePaths.Add(FullPath);
    }


    // 一覧を並べ替え
    OutFilePaths.Sort();


    // ------------------------------------
    // 5. 結果を返す
    // ------------------------------------

    OutMessage = FString::Printf(
        TEXT(
            "%d件のプリセットを取得しました。"
        ),
        OutFilePaths.Num()
    );

    return true;
}


// ========================================
// DeletePreset
// 指定したプリセットを削除
// ========================================

bool FBXImportPresetService::DeletePreset(
    const FString& PresetName,
    FString& OutMessage
)
{
    OutMessage.Empty();


    // ------------------------------------
    // 1. プリセット名を検証
    // ------------------------------------

    FString CleanPresetName;
    FString SafeFileName;

    if (!PreparePresetName(
        PresetName,
        CleanPresetName,
        SafeFileName,
        OutMessage
    ))
    {
        return false;
    }


    // ------------------------------------
    // 2. 共通保存先を取得
    // ------------------------------------

    const FString PresetDirectory =
        GetPresetDirectory();


    // ------------------------------------
    // 3. 削除対象のパスを作成
    // ------------------------------------

    const FString FilePath = FPaths::Combine(
        PresetDirectory,
        SafeFileName + TEXT(".json")
    );


    // ------------------------------------
    // 4. ファイルの存在を確認
    // ------------------------------------

    if (!FPaths::FileExists(FilePath))
    {
        OutMessage = TEXT("削除するプリセットが見つかりません。");
        return false;
    }


    // ------------------------------------
    // 5. ファイルを削除
    // ------------------------------------

    if (!IFileManager::Get().Delete(
        *FilePath,
        true,
        false,
        false
    ))
    {
        OutMessage = TEXT("プリセットの削除に失敗しました。");
        return false;
    }


    OutMessage = TEXT("プリセットを削除しました。");

    return true;
}