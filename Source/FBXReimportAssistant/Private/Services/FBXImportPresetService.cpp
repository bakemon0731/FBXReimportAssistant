#include "Services/FBXImportPresetService.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

bool FBXImportPresetService::SavePreset(const FFBXImportPreset& Preset, bool bOverwriteExisting, FString& OutFilePath, FString& OutMessage)
{
	OutFilePath.Empty();
	OutMessage.Empty();
	
	// 保存処理を実装
	FString PresetName = Preset.Name;
	
	PresetName.TrimStartAndEndInline();
	
	if (PresetName.IsEmpty())
	{
		OutMessage = TEXT("プリセット名が入力されていません。");
		return false;
	}
	
	// ファイル名に使えない文字が含まれていた場合、置き換えて安全な名前にする
	const FString SafeFileName = FPaths::MakeValidFileName(PresetName, TCHAR('_'));
	
	// ルートオブジェクト
	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	// 名前とバージョンを設定
	RootObject->SetStringField(TEXT("Name"), PresetName);
	RootObject->SetNumberField(TEXT("Version"), Preset.Version);
	
	// Settingsオブジェクト
	TSharedRef<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
	//FFBXBatchOverrideSettings の情報を格納
	const FFBXBatchOverrideSettings& Settings = Preset.Settings;
	SettingsObject->SetBoolField(TEXT("OverrideTranslation"),Settings.bOverrideTranslation);
	SettingsObject->SetBoolField(TEXT("OverrideRotation"),Settings.bOverrideRotation);
	SettingsObject->SetBoolField(TEXT("OverrideUniformScale"),Settings.bOverrideUniformScale);
	
	// Translationを保存する入れ子のJSONオブジェクト
	TSharedRef<FJsonObject> TranslationObject = MakeShared<FJsonObject>();
	
	TranslationObject->SetNumberField(TEXT("X"),Settings.Translation.X);
	TranslationObject->SetNumberField(TEXT("Y"),Settings.Translation.Y);
	TranslationObject->SetNumberField(TEXT("Z"),Settings.Translation.Z);
	
	SettingsObject->SetObjectField(TEXT("Translation"), TranslationObject.ToSharedPtr());
	
	// Rotationを保存する入れ子のJSONオブジェクト
	TSharedRef<FJsonObject> RotationObject = MakeShared<FJsonObject>();
	
	RotationObject->SetNumberField(TEXT("Pitch"),Settings.Rotation.Pitch);
	RotationObject->SetNumberField(TEXT("Yaw"),Settings.Rotation.Yaw);
	RotationObject->SetNumberField(TEXT("Roll"),Settings.Rotation.Roll);
	
	SettingsObject->SetObjectField(TEXT("Rotation"), RotationObject.ToSharedPtr());
	
	// Uniform Scaleを保存
	SettingsObject->SetNumberField(TEXT("UniformScale"),Settings.UniformScale);
	
	RootObject->SetObjectField(TEXT("Settings"), SettingsObject.ToSharedPtr());
	
	// JSON文字列に変換し、ファイルに書き出せる文字列にする
	FString JsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(RootObject, Writer))
	{
		OutMessage = TEXT("JSONへの変換に失敗しました。");
		return false;
	}
	
	// 保存先のフォルダを作成
	const FString PresetDirectory = FPaths::Combine(
		FPaths::ProjectConfigDir(),
		TEXT("FBXReimportAssistant"),
		TEXT("Presets"));
	IFileManager::Get().MakeDirectory(*PresetDirectory,true);
	
	// フォルダが作成できたか確認
	if (!FPaths::DirectoryExists(PresetDirectory))
	{
		OutMessage = TEXT("保存先フォルダを作成できませんでした。");
		return false;
	}
	
	// 保存先のファイルパスを作る
	const FString FilePath = FPaths::Combine(PresetDirectory,SafeFileName + TEXT(".json"));
	
	// 同名ファイルが存在している場合の安全対策を追加
	if (FPaths::FileExists(FilePath) && !bOverwriteExisting)
	{
		OutMessage = TEXT("同名のプリセットが既に存在します。");
		return false;
	}
	
	// 最後にファイルへ書き出し
	if (!FFileHelper::SaveStringToFile(JsonString,*FilePath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutMessage = TEXT("プリセットの保存に失敗しました。");
		return false;
	}
	
	OutFilePath = FilePath;
	OutMessage = TEXT("プリセットを保存しました。");
	
	return true;
}

bool FBXImportPresetService::LoadPreset(const FString& FilePath, FFBXImportPreset& OutPreset, FString& OutMessage)
{
	OutPreset = FFBXImportPreset();
	OutMessage.Empty();
	
	// ファイルの存在を確認
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
	
	// JSONファイルを文字列として読み込む
	FString JsonString;
	
	if (!FFileHelper::LoadFileToString(JsonString,*FilePath))
	{
		OutMessage = TEXT("プリセットファイルの読み込みに失敗しました。");
		return false;
	}
	
	// JSON文字列をオブジェクトに変換
	TSharedPtr<FJsonObject> RootObject;
	
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutMessage = TEXT("JSONの解析に失敗しました。");
		return false;
	}
	
	// プリセットの基本情報を取得
	FFBXImportPreset LoadedPreset;
	if (!RootObject->TryGetStringField(TEXT("Name"), LoadedPreset.Name))
	{
		OutMessage = TEXT("プリセット名が存在しません。");
		return false;
	}
	
	if (!RootObject->TryGetNumberField(TEXT("Version"), LoadedPreset.Version))
	{
		OutMessage = TEXT("バージョン情報が存在しません。");
		return false;
	}
	
	// 名前とバージョンの検証を追加
	LoadedPreset.Name.TrimStartAndEndInline();
	
	if (LoadedPreset.Name.IsEmpty())
	{
		OutMessage = TEXT("プリセット名が空です。");
		return false;
	}
	
	if (LoadedPreset.Version != 1)
	{
		OutMessage = TEXT("未対応のプリセットバージョンです。");
		return false;
	}
	
	// 親となる Settings オブジェクトを取得
	const TSharedPtr<FJsonObject>* SettingsObjectPtr = nullptr;
	
	// 指定したキーがJSONオブジェクトとして存在するかを確認し、そのオブジェクトを取得
	if (!RootObject->TryGetObjectField(TEXT("Settings"), SettingsObjectPtr) || SettingsObjectPtr == nullptr || !SettingsObjectPtr->IsValid())
	{
		OutMessage = TEXT("Settingsが存在しません。");
		return false;
	}
	
	const TSharedPtr<FJsonObject>& SettingsObject = *SettingsObjectPtr;
	
	// チェック状態を復元
	bool bOverrideTranslation = false;
	bool bOverrideRotation = false;
	bool bOverrideUniformScale = false;

	if (!SettingsObject->TryGetBoolField(TEXT("OverrideTranslation"),bOverrideTranslation
		) || !SettingsObject->TryGetBoolField(TEXT("OverrideRotation"),bOverrideRotation
			) || !SettingsObject->TryGetBoolField(TEXT("OverrideUniformScale"),bOverrideUniformScale))
	{
		OutMessage = TEXT("設定のチェック状態を読み込めません。");
		return false;
	}
	
	// Translationを復元
	const TSharedPtr<FJsonObject>* TranslationObjectPtr = nullptr;
	
	if (!SettingsObject->TryGetObjectField(TEXT("Translation"), TranslationObjectPtr) || TranslationObjectPtr == nullptr || !TranslationObjectPtr->IsValid())
	{
		OutMessage = TEXT("Translationのデータがありません。");
		return false;
	}
	
	const TSharedPtr<FJsonObject>& TranslationObject = *TranslationObjectPtr;
	
	// X・Y・Zを取得
	double TranslationX = 0.0;
	double TranslationY = 0.0;
	double TranslationZ = 0.0;

	if (!TranslationObject->TryGetNumberField(TEXT("X"),TranslationX
		) || !TranslationObject->TryGetNumberField(TEXT("Y"),TranslationY
			) || !TranslationObject->TryGetNumberField(TEXT("Z"),TranslationZ))
	{
		OutMessage = TEXT("Translationの数値を読み込めません。");
		return false;
	}
	
	// 値が正常か確認
	if (!FMath::IsFinite(TranslationX) ||!FMath::IsFinite(TranslationY) ||!FMath::IsFinite(TranslationZ))
	{
		OutMessage = TEXT("Translationに不正な数値があります。");
		return false;
	}
	
	// Rotationを復元
	const TSharedPtr<FJsonObject>* RotationObjectPtr = nullptr;

	if (!SettingsObject->TryGetObjectField(TEXT("Rotation"),RotationObjectPtr) || RotationObjectPtr == nullptr ||!RotationObjectPtr->IsValid())
	{
		OutMessage = TEXT("Rotationのデータがありません。");
		return false;
	}
	
	const TSharedPtr<FJsonObject>& RotationObject = *RotationObjectPtr;
	
	// X・Y・Zを取得
	double RotationPitch = 0.0;
	double RotationYaw = 0.0;
	double RotationRoll = 0.0;
	
	if (!RotationObject->TryGetNumberField(TEXT("Pitch"),RotationPitch
		) || !RotationObject->TryGetNumberField(TEXT("Yaw"),RotationYaw
			) || !RotationObject->TryGetNumberField(TEXT("Roll"),RotationRoll))
	{
		OutMessage = TEXT("Rotationの数値を読み込めません。");
		return false;
	}
	
	// 取得後、不正な数値をチェック
	if (!FMath::IsFinite(RotationPitch) || !FMath::IsFinite(RotationYaw) || !FMath::IsFinite(RotationRoll))
	{
		OutMessage = TEXT("Rotationに不正な数値があります。");
		return false;
	}
	
	// Uniform Scaleを復元
	float UniformScale = 1.0f;
	
	if (!SettingsObject->TryGetNumberField(TEXT("UniformScale"),UniformScale))
	{
		OutMessage = TEXT("Uniform Scaleを読み込めません。");
		return false;
	}
	
	// 値を検証
	if (!FMath::IsFinite(UniformScale) || UniformScale == 0.0f)
	{
		OutMessage = TEXT("Uniform Scaleの値が不正です。");
		return false;
	}
	
	// C++の構造体に戻す
	// チェック状態を代入
	LoadedPreset.Settings.bOverrideTranslation = bOverrideTranslation;
	LoadedPreset.Settings.bOverrideRotation = bOverrideRotation;
	LoadedPreset.Settings.bOverrideUniformScale = bOverrideUniformScale;
	
	// Translationを復元
	LoadedPreset.Settings.Translation = FVector(TranslationX, TranslationY, TranslationZ);
	
	// Rotationを復元
	LoadedPreset.Settings.Rotation = FRotator(RotationPitch, RotationYaw, RotationRoll);
	
	// Scaleを復元
	LoadedPreset.Settings.UniformScale = UniformScale;
	
	// 読み込み結果を呼び出し元に返す。
	OutPreset = MoveTemp(LoadedPreset);
	
	OutMessage = TEXT("プリセットを読み込みました。");
	
	return true;
}

bool FBXImportPresetService::GetPresetFiles(TArray<FString>& OutFilePaths, FString& OutMessage)
{
	//　配列の中身を空にする
	OutFilePaths.Reset();
	OutMessage.Empty();
	
	// プロジェクトのConfig以下にあるPresetsフォルダを指定し取得
	const FString PresetDirectory = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("FBXReimportAssistant"),TEXT("Presets"));
	
	// フォルダの存在を確認
	if (!FPaths::DirectoryExists(PresetDirectory))
	{
		OutMessage = TEXT("プリセット保存フォルダが存在しません。");
		// 正常に取得できたがプリセットが0件だった可能性があるのでtrueを返す
		return true;
	}
	
	// ファイル名の一覧を取得
	TArray<FString> FoundFileNames;
	// 指定したフォルダの中から、拡張子がJSONのファイルだけを取得
	IFileManager::Get().FindFiles(FoundFileNames,*PresetDirectory,TEXT("json"));
	
	// ファイル名だけではなく、フルパスに変換して返す
	for (const FString& FileName : FoundFileNames)
	{
		const FString FullPath = FPaths::Combine(PresetDirectory, FileName);
		
		OutFilePaths.Add(FullPath);
	}
	
	OutMessage = FString::Printf(
		TEXT(
			"検索先：%s\n"
			"取得件数：%d"
			),
			*PresetDirectory,
			OutFilePaths.Num());
	return true;
}

bool FBXImportPresetService::DeletePreset(const FString& PresetName, FString& OutMessage)
{
	OutMessage.Empty();
	
	// プリセット名を検証
	FString CleanPresetName = PresetName;
	
	CleanPresetName.TrimStartAndEndInline();

	if (CleanPresetName.IsEmpty())
	{
		OutMessage = TEXT("削除するプリセット名がありません。");
		return false;
	}
	
	// ファイル名を安全な形に変換
	const FString SafeFileName = FPaths::MakeValidFileName(CleanPresetName,TCHAR('_'));
	
	// 削除対象のパスを作成
	const FString PresetDirectory = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("FBXReimportAssistant"), TEXT("Presets"));
	
	const FString FilePath = FPaths::Combine(PresetDirectory, SafeFileName + TEXT(".json"));
	
	// ファイルが存在するか確認
	if (!FPaths::FileExists(FilePath))
	{
		OutMessage = TEXT("削除するプリセットが見つかりません。");
		return false;
	}
	
	// 実際に削除
	if (!IFileManager::Get().Delete(*FilePath, true, false, false))
	{
		OutMessage = TEXT("プリセットの削除に失敗しました。");
		return false;
	}
	
	OutMessage = TEXT("プリセットを削除しました。");
	return true;
}
