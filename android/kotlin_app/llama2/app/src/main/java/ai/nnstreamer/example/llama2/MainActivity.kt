package ai.nnstreamer.example.llama2

import android.annotation.SuppressLint
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Message
import android.os.Messenger
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import ai.nnstreamer.example.llama2.ui.theme.Llama2Theme

@SuppressLint("HandlerLeak")
class MainActivity : ComponentActivity() {
    private var mService: Messenger? = null
    private val result = mutableStateOf("")

    private val handler = object : Handler() {
        override fun handleMessage(msg: Message) {
            result.value = msg.data.getString("response").toString()
        }
    }

    private val messenger = Messenger(handler)

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            mService = Messenger(service)
        }

        override fun onServiceDisconnected(arg0: ComponentName) {
            mService = null
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        Intent().apply {
            setClassName(
                "ai.nnstreamer.ml.inference.offloading",
                "ai.nnstreamer.ml.inference.offloading.MainService"
            )
        }.also {
            bindService(it, connection, Context.BIND_AUTO_CREATE)
        }

        setContent {
            Llama2Theme {
                Surface(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(16.dp),
                    color = MaterialTheme.colorScheme.background
                ) {
                    Column(
                        modifier = Modifier.height(48.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        var input by remember { mutableStateOf("") }

                        Spacer(modifier = Modifier.height(50.dp))
                        Text(
                            text = "Run llama2",
                            modifier = Modifier
                                .clickable {
                                    // todo: Generalize the message type
                                    val message = Message.obtain(null, 5)
                                    val bundle = Bundle()
                                    bundle.putString("input", input);

                                    message.data = bundle
                                    message.replyTo = messenger

                                    mService?.send(
                                        message
                                    )
                                }
                                .background(Color.LightGray)
                                .padding(8.dp),
                            fontSize = 16.sp
                        )

                        Spacer(modifier = Modifier.height(50.dp))

                        OutlinedTextField(
                            value = input,
                            onValueChange = { input = it },
                            label = { Text("input prompt") }
                        )

                        Spacer(modifier = Modifier.height(50.dp))

                        val output by result

                        BasicTextField(
                            value = output,
                            onValueChange = {},
                            modifier = Modifier.fillMaxWidth(),
                            textStyle = TextStyle(
                                fontSize = 16.sp
                            ),
                            singleLine = false,
                        )
                    }
                }
            }
        }
    }
}
