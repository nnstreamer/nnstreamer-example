# Gtreamer Installing for Android development

## Ubuntu version
- ubuntu-18.04.5-desktop-amd64.iso
- VMware RAM 8GB / processor 2 / capacity 80GB

## Setting

#### Install Chrome
- reference : <https://somjang.tistory.com/27>

#### Install git
`sudo apt-get install git`

## Course 
0. Full reference
- <http://blog.naver.com/PostView.nhn?blogId=chandong83&logNo=220967030568&categoryNo=0&parentCategoryNo=32&viewDate=&currentPage=1&postListTopCurrentPage=1&from=postView>

## Caution in Course
1. Download Android SDK
- reference : <https://webnautes.tistory.com/1134>
- __latest version__

2. Download NDK
- search android ndk download in chrome. (official site)
- __latest version__

3. ndk-build error
- when you meet ndk-build error, try this.
- copy link : <https://awesomeopensource.com/project/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c>

`sudo apt-get install libssl-dev libcurl4-openssl-dev liblog4cplus-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-tools`

- AndroidManifest.xml minsdk version __9 -> 16__ change
- delete __armeabi ABI__ 

4. Binary version : 1.18.x


## Setting Tip (from Team)
```sh
오류 01 : Android SDK Error
해결 : build.gradle에 오른쪽 마우스 버튼 -> New -> File 클릭 -> 파일 이름 : settings.gradle
settings.gradle
include ':app'

오류 02 : apply plugin Error
해결 : build.gradle 내용 수정
build.gradle
buildscript {
    repositories {
        jcenter() // or mavenCentral()
        mavenCentral()
        maven{url "https://maven.google.com"}
        google()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:3.1.4'
    }
}
allprojects {
    repositories{
        google()
        jcenter()
    }
}
//(JSP)저의 경우 이게 없으면 안됬습니다
apply plugin: 'com.android.application'

중략

dependencies {
    implementation fileTree(dir: 'libs', include: ['*.jar'])
    // testImplementation 'junit:junit:4.12'
    // implementation 'com.android.support:appcompat-v7:23.1.1'
}

오류 03 : NDK Error
해결 : File -> Project Structure 메뉴 클릭 -> SDK Location -> Android NDK location 설정


오류 04 : gstAndroidRoot 쪽 에러
gradle.properties 파일 생성후 
gstAndroidRoot= gst 경로 설정
```


## Review
- Virtual Machine보다 VMware가 훨씬 개발하기 편리함
- 개발에 속도를 올려야겠다.
