# Gtreamer Installing for Android development

## Ubuntu version
- ubuntu-18.04.5-desktop-amd64.iso
- Virtual box

## Setting

#### file name to English 
- reference : <https://hcnam.tistory.com/29>

#### Install Chrome
- reference : <https://somjang.tistory.com/27>

#### Korean/English onActive
- reference : <https://webnautes.tistory.com/1199>

#### Host key set 'ctrl + shift'
- if you want your own surrogate key, set free
- reference : <https://m.blog.naver.com/PostView.nhn?blogId=gingsero&logNo=220611873540&proxyReferer=https:%2F%2Fwww.google.com%2F>

#### Install git
`apt-get install git`

## Course (not answer)
0. Full reference
- <http://blog.naver.com/PostView.nhn?blogId=chandong83&logNo=220967030568&categoryNo=0&parentCategoryNo=32&viewDate=&currentPage=1&postListTopCurrentPage=1&from=postView>

## Caution in Course
1. Download Android SDK
- reference : <https://webnautes.tistory.com/1134>
- __latest version__

2. Download NDK
- search android ndk download in chrome. (official site)
- __r18b version__

3. ndk-build error
- when you meet ndk-build error, try this.
- copy link : <https://awesomeopensource.com/project/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c>

`sudo apt-get install libssl-dev libcurl4-openssl-dev liblog4cplus-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-tools`

- AndroidManifest.xml minsdk version __9 -> 16__ change
- delete __armeabi ABI__ 

4. Binary version : 1.16.2


## Review
- Virtual Machine과 네이티브 우분투 서둘러 결정하여 개발할 것.
- 튜토리얼 역시 쉽지 않았기에 더 노력을 투자해야 할 것 같다.
